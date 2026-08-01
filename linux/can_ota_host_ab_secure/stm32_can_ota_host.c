#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "ota_package.h"

#define DEFAULT_CAN_IFACE "can0"
#define DEFAULT_PACING_US 1000u
#define DEFAULT_STATUS_TIMEOUT_MS 15000
#define ENTER_ATTEMPTS 12
#define ENTER_PROBE_TIMEOUT_MS 1000
#define APP_CONFIRM_TIMEOUT_MS 15000
#define DATA_BYTES_PER_FRAME 6u
#define AUTH_PACING_US 5000u
#define BOOTLOADER_SESSION_SETTLE_US 100000u

typedef struct {
    uint8_t status;
    uint8_t error;
    uint8_t progress;
    uint16_t seq;
    uint16_t received_kb;
    uint8_t target_slot;
} ota_status_t;

static void usage(const char *program)
{
    printf("Usage: %s -f firmware.ota3 [-i can0] [-p pacing_us] "
           "[-t timeout_ms]", program);
#ifdef CAN_OTA_ACCEPTANCE_TEST
    printf(" [-H]");
#endif
    printf("\n");
#ifdef CAN_OTA_ACCEPTANCE_TEST
    printf("  -H  acceptance test only: send a mismatched Hardware ID "
           "manifest to the STM32\n");
#endif
}

static const char *status_name(uint8_t status)
{
    switch (status) {
    case CAN_OTA_AB_STATUS_READY: return "ready";
    case CAN_OTA_AB_STATUS_ERASING: return "erasing";
    case CAN_OTA_AB_STATUS_WRITING: return "writing";
    case CAN_OTA_AB_STATUS_VERIFY: return "verify";
    case CAN_OTA_AB_STATUS_DONE: return "done";
    case CAN_OTA_AB_STATUS_ROLLBACK: return "rollback";
    case CAN_OTA_AB_STATUS_ERROR: return "error";
    default: return "unknown";
    }
}

static const char *error_name(uint8_t error)
{
    switch (error) {
    case CAN_OTA_AB_ERR_NONE: return "none";
    case CAN_OTA_AB_ERR_TIMEOUT: return "timeout";
    case CAN_OTA_AB_ERR_SIZE: return "image-size";
    case CAN_OTA_AB_ERR_SEQ: return "sequence";
    case CAN_OTA_AB_ERR_FLASH: return "flash";
    case CAN_OTA_AB_ERR_CRC: return "crc32";
    case CAN_OTA_AB_ERR_APP: return "app-vector";
    case CAN_OTA_AB_ERR_MANIFEST: return "manifest";
    case CAN_OTA_AB_ERR_HARDWARE: return "hardware-id";
    case CAN_OTA_AB_ERR_ROLLBACK_POLICY: return "rollback-policy";
    case CAN_OTA_AB_ERR_METADATA: return "metadata";
    case CAN_OTA_AB_ERR_SHA256: return "sha256";
    case CAN_OTA_AB_ERR_SIGNATURE: return "ecdsa-signature";
    case CAN_OTA_AB_ERR_KEY_ID: return "key-id";
    case CAN_OTA_AB_ERR_SLOT: return "slot";
    case CAN_OTA_AB_ERR_BLOCK_HASH: return "block-sha256";
    case CAN_OTA_AB_ERR_RESUME: return "resume-journal";
    default: return "unknown";
    }
}

static const char *slot_name(uint8_t slot)
{
    if (slot == CAN_OTA_AB_SLOT_A) {
        return "A";
    }
    if (slot == CAN_OTA_AB_SLOT_B) {
        return "B";
    }
    return "?";
}

static void put_le16(uint8_t *buffer, uint16_t value)
{
    buffer[0] = (uint8_t)value;
    buffer[1] = (uint8_t)(value >> 8);
}

static uint16_t get_le16(const uint8_t *buffer)
{
    return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
}

static int64_t monotonic_ms(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
        return -1;
    }
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int load_file(const char *path, uint8_t **data_out, size_t *size_out)
{
    FILE *stream = fopen(path, "rb");
    long length;
    uint8_t *data;

    if (stream == NULL) {
        perror("fopen package");
        return -1;
    }
    if (fseek(stream, 0, SEEK_END) != 0 ||
        (length = ftell(stream)) <= 0) {
        fprintf(stderr, "invalid package length\n");
        fclose(stream);
        return -1;
    }
    rewind(stream);
    data = malloc((size_t)length);
    if (data == NULL) {
        fclose(stream);
        return -1;
    }
    if (fread(data, 1u, (size_t)length, stream) != (size_t)length) {
        perror("fread package");
        free(data);
        fclose(stream);
        return -1;
    }
    fclose(stream);
    *data_out = data;
    *size_out = (size_t)length;
    return 0;
}

static int open_can_socket(const char *interface)
{
    int fd;
    struct ifreq ifr;
    struct sockaddr_can address;
    struct can_filter filters[] = {
        { CAN_OTA_AB_ID_STATUS,
          CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG },
        { CAN_APP_ID_HEARTBEAT,
          CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG }
    };

    fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0) {
        perror("socket CAN");
        return -1;
    }
    if (setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER,
                   filters, sizeof(filters)) < 0) {
        perror("setsockopt CAN filter");
        close(fd);
        return -1;
    }
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", interface);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl SIOCGIFINDEX");
        close(fd);
        return -1;
    }
    memset(&address, 0, sizeof(address));
    address.can_family = AF_CAN;
    address.can_ifindex = ifr.ifr_ifindex;
    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind CAN");
        close(fd);
        return -1;
    }
    return fd;
}

static int send_frame(int fd, uint32_t id,
                      const uint8_t *data, uint8_t length)
{
    struct can_frame frame;

    if (length > CAN_MAX_DLEN) {
        errno = EINVAL;
        return -1;
    }
    memset(&frame, 0, sizeof(frame));
    frame.can_id = id;
    frame.can_dlc = length;
    if (length != 0u) {
        memcpy(frame.data, data, length);
    }
    if (write(fd, &frame, sizeof(frame)) != (ssize_t)sizeof(frame)) {
        perror("write CAN");
        return -1;
    }
    return 0;
}

static int receive_status(int fd, ota_status_t *status, int timeout_ms)
{
    while (1) {
        fd_set read_set;
        struct timeval timeout;
        struct can_frame frame;
        int result;

        FD_ZERO(&read_set);
        FD_SET(fd, &read_set);
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        result = select(fd + 1, &read_set, NULL, NULL, &timeout);
        if (result <= 0) {
            return result;
        }
        result = (int)read(fd, &frame, sizeof(frame));
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if ((size_t)result != sizeof(frame) ||
            (frame.can_id & (CAN_EFF_FLAG | CAN_RTR_FLAG)) != 0u ||
            (frame.can_id & CAN_SFF_MASK) != CAN_OTA_AB_ID_STATUS ||
            frame.can_dlc != 8u) {
            continue;
        }
        status->status = frame.data[0];
        status->error = frame.data[1];
        status->progress = frame.data[2];
        status->seq = get_le16(frame.data + 3u);
        status->received_kb = get_le16(frame.data + 5u);
        status->target_slot = frame.data[7];
        return 1;
    }
}

static int wait_status(int fd, ota_status_t *status, int timeout_ms)
{
    int result = receive_status(fd, status, timeout_ms);
    if (result == 0) {
        fprintf(stderr, "timeout waiting for STM32 OTA status\n");
        return -1;
    }
    if (result < 0) {
        perror("receive OTA status");
        return -1;
    }
    printf("STM32 status=%s error=%s progress=%u%% seq=%u "
           "received=%uKB target=%s\n",
           status_name(status->status), error_name(status->error),
           status->progress, status->seq, status->received_kb,
           slot_name(status->target_slot));
    if (status->status == CAN_OTA_AB_STATUS_ERROR) {
        fprintf(stderr, "STM32 rejected OTA: %s (0x%02X)\n",
                error_name(status->error), status->error);
        return -1;
    }
    return 0;
}

static int enter_bootloader(int fd, int timeout_ms, uint8_t *target_slot)
{
    uint8_t app_command[8] = {
        CAN_APP_CMD_ENTER_BOOT, 0u, 0u, 0u, 0u, 0u, 0u,
        CAN_APP_CMD_ENTER_BOOT
    };
    uint8_t enter[8] = { CAN_OTA_AB_CMD_ENTER, 0u, 0u, 0u,
                         0u, 0u, 0u, 0u };
    int probe_timeout = timeout_ms < ENTER_PROBE_TIMEOUT_MS ?
                        timeout_ms : ENTER_PROBE_TIMEOUT_MS;
    int attempt;

    printf("Requesting running App to enter Bootloader...\n");
    if (send_frame(fd, CAN_APP_ID_CONTROL,
                   app_command, sizeof(app_command)) < 0) {
        return -1;
    }
    usleep(100000u);

    for (attempt = 0; attempt < ENTER_ATTEMPTS; attempt++) {
        ota_status_t status;
        if (send_frame(fd, CAN_OTA_AB_ID_ENTER, enter, sizeof(enter)) < 0) {
            return -1;
        }
        if (receive_status(fd, &status, probe_timeout) > 0 &&
            status.status == CAN_OTA_AB_STATUS_READY &&
            (status.target_slot == CAN_OTA_AB_SLOT_A ||
             status.target_slot == CAN_OTA_AB_SLOT_B)) {
            *target_slot = status.target_slot;
            printf("Bootloader ready; inactive target is Slot %s.\n",
                   slot_name(*target_slot));
            /*
             * The Bootloader reports READY from its enter-command wait loop,
             * then prints the session banner before it starts draining AUTH
             * frames.  Give that polling loop time to become active so the
             * STM32F1 three-frame RX FIFO cannot overflow at session start.
             */
            usleep(BOOTLOADER_SESSION_SETTLE_US);
            return 0;
        }
    }
    fprintf(stderr, "STM32 did not enter A/B OTA mode\n");
    return -1;
}

static int send_chunked_payload(int fd, uint32_t can_id,
                                const uint8_t *data, size_t size)
{
    size_t offset = 0u;
    uint16_t sequence = 0u;

    while (offset < size) {
        uint8_t frame[8];
        size_t remaining = size - offset;
        size_t chunk = remaining >= DATA_BYTES_PER_FRAME ?
                       DATA_BYTES_PER_FRAME : remaining;
        memset(frame, 0xFF, sizeof(frame));
        put_le16(frame, sequence++);
        memcpy(frame + 2u, data + offset, chunk);
        if (send_frame(fd, can_id, frame, sizeof(frame)) < 0) {
            return -1;
        }
        offset += chunk;
        usleep(AUTH_PACING_US);
    }
    return 0;
}

static int send_authenticated_manifest(
    int fd, const ota_ab_package_t *package, uint8_t target_slot,
    int timeout_ms, size_t *resume_offset)
{
    ota_status_t status;
    size_t table_size;

    printf("Sending signed OTA3 manifest (%u bytes)...\n",
           CAN_OTA_AB_PACKAGE_HEADER_SIZE);
    if (send_chunked_payload(fd, CAN_OTA_AB_ID_AUTH, package->header,
                             CAN_OTA_AB_PACKAGE_HEADER_SIZE) < 0) {
        return -1;
    }
    while (1) {
        if (wait_status(fd, &status, timeout_ms) < 0) {
            return -1;
        }
        if (status.status == CAN_OTA_AB_STATUS_READY) {
            break;
        }
    }
    table_size = (size_t)package->block_counts[target_slot] *
                 CAN_OTA_AB_SHA256_SIZE;
    printf("Sending authenticated Slot %s block hash table "
           "(%zu blocks, %zu bytes)...\n",
           slot_name(target_slot),
           (size_t)package->block_counts[target_slot], table_size);
    if (send_chunked_payload(fd, CAN_OTA_AB_ID_BLOCK_HASH,
                             package->block_hash_tables[target_slot],
                             table_size) < 0) {
        return -1;
    }
    while (1) {
        if (wait_status(fd, &status, timeout_ms) < 0) {
            return -1;
        }
        if (status.status == CAN_OTA_AB_STATUS_WRITING) {
            *resume_offset = status.progress == 100u ?
                             package->image_sizes[target_slot] :
                             (size_t)status.received_kb * 1024u;
            if (*resume_offset > package->image_sizes[target_slot] ||
                (*resume_offset != package->image_sizes[target_slot] &&
                 (*resume_offset % CAN_OTA_AB_BLOCK_SIZE) != 0u)) {
                fprintf(stderr, "invalid resume offset reported by STM32\n");
                return -1;
            }
            return 0;
        }
        if (status.status != CAN_OTA_AB_STATUS_ERASING &&
            status.status != CAN_OTA_AB_STATUS_READY) {
            fprintf(stderr, "unexpected status after signed manifest\n");
            return -1;
        }
    }
}

static int send_image(int fd, const uint8_t *image, size_t size,
                      size_t start_offset,
                      unsigned int pacing_us, int timeout_ms)
{
    size_t offset = start_offset;
    uint16_t sequence = 0u;

    while (offset < size) {
        uint8_t frame[8];
        size_t remaining = size - offset;
        size_t block_remaining =
            CAN_OTA_AB_BLOCK_SIZE -
            (offset % CAN_OTA_AB_BLOCK_SIZE);
        size_t chunk = remaining >= DATA_BYTES_PER_FRAME ?
                       DATA_BYTES_PER_FRAME : remaining;
        if (chunk > block_remaining) {
            chunk = block_remaining;
        }
        memset(frame, 0xFF, sizeof(frame));
        put_le16(frame, sequence);
        memcpy(frame + 2u, image + offset, chunk);
        if (send_frame(fd, CAN_OTA_AB_ID_DATA, frame,
                       (uint8_t)(chunk + 2u)) < 0) {
            return -1;
        }
        offset += chunk;
        if (pacing_us != 0u) {
            usleep(pacing_us);
        }
        if (sequence == 0u || (sequence % 32u) == 0u ||
            (offset % CAN_OTA_AB_BLOCK_SIZE) == 0u || offset == size) {
            ota_status_t status;
            if (wait_status(fd, &status, timeout_ms) < 0) {
                return -1;
            }
        }
        sequence++;
    }

    while (1) {
        ota_status_t status;
        if (wait_status(fd, &status, timeout_ms) < 0) {
            return -1;
        }
        if (status.status == CAN_OTA_AB_STATUS_DONE) {
            return 0;
        }
        if (status.status != CAN_OTA_AB_STATUS_WRITING &&
            status.status != CAN_OTA_AB_STATUS_VERIFY) {
            fprintf(stderr, "unexpected final OTA status\n");
            return -1;
        }
    }
}

static int wait_app_confirmation(int fd, uint32_t version,
                                 uint8_t expected_slot)
{
    int64_t deadline = monotonic_ms() + APP_CONFIRM_TIMEOUT_MS;

    while (monotonic_ms() < deadline) {
        fd_set read_set;
        struct timeval timeout = { 1, 0 };
        struct can_frame frame;
        uint8_t checksum = 0u;
        int result;
        int index;

        FD_ZERO(&read_set);
        FD_SET(fd, &read_set);
        result = select(fd + 1, &read_set, NULL, NULL, &timeout);
        if (result <= 0) {
            continue;
        }
        result = (int)read(fd, &frame, sizeof(frame));
        if ((size_t)result != sizeof(frame) ||
            (frame.can_id & (CAN_EFF_FLAG | CAN_RTR_FLAG)) != 0u ||
            (frame.can_id & CAN_SFF_MASK) != CAN_APP_ID_HEARTBEAT ||
            frame.can_dlc != 8u) {
            continue;
        }
        for (index = 0; index < 7; index++) {
            checksum = (uint8_t)(checksum + frame.data[index]);
        }
        if (checksum != frame.data[7] ||
            frame.data[0] != CAN_OTA_VERSION_MAJOR(version) ||
            frame.data[1] != CAN_OTA_VERSION_MINOR(version) ||
            frame.data[4] != CAN_OTA_VERSION_PATCH(version) ||
            frame.data[5] != CAN_OTA_VERSION_BUILD(version) ||
            frame.data[6] != expected_slot) {
            continue;
        }
        printf("Confirmed heartbeat %u.%u.%u.%u from Slot %s received.\n",
               frame.data[0], frame.data[1], frame.data[4], frame.data[5],
               slot_name(frame.data[6]));
        return 0;
    }
    fprintf(stderr, "timeout waiting for matching confirmed App heartbeat\n");
    return -1;
}

int main(int argc, char **argv)
{
    const char *interface = DEFAULT_CAN_IFACE;
    const char *package_path = NULL;
    unsigned int pacing_us = DEFAULT_PACING_US;
    int timeout_ms = DEFAULT_STATUS_TIMEOUT_MS;
    uint8_t *file_data = NULL;
    size_t file_size = 0u;
    ota_ab_package_t package;
    char parse_error[192];
    uint8_t target_slot;
    size_t resume_offset = 0u;
    int can_fd = -1;
    int allow_hardware_mismatch = 0;
    int option;
    int result = 1;

#ifdef CAN_OTA_ACCEPTANCE_TEST
    const char *option_string = "i:f:p:t:hH";
#else
    const char *option_string = "i:f:p:t:h";
#endif

    while ((option = getopt(argc, argv, option_string)) != -1) {
        switch (option) {
        case 'i': interface = optarg; break;
        case 'f': package_path = optarg; break;
        case 'p': pacing_us = (unsigned int)strtoul(optarg, NULL, 10); break;
        case 't': timeout_ms = atoi(optarg); break;
#ifdef CAN_OTA_ACCEPTANCE_TEST
        case 'H': allow_hardware_mismatch = 1; break;
#endif
        case 'h': usage(argv[0]); return 0;
        default: usage(argv[0]); return 1;
        }
    }
    if (package_path == NULL) {
        usage(argv[0]);
        return 1;
    }
    if (load_file(package_path, &file_data, &file_size) < 0) {
        goto out;
    }
    if (ota_ab_package_parse(file_data, file_size, &package,
                             parse_error, sizeof(parse_error)) < 0) {
        fprintf(stderr, "invalid OTA3 package: %s\n", parse_error);
        goto out;
    }
    if (package.hardware_id != CAN_OTA_HARDWARE_ID_STM32F103 &&
        !allow_hardware_mismatch) {
        fprintf(stderr, "package hardware ID does not match STM32F103\n");
        goto out;
    }
    if (package.hardware_id != CAN_OTA_HARDWARE_ID_STM32F103) {
        fprintf(stderr,
                "WARNING: acceptance-test override sends mismatched "
                "Hardware ID 0x%04X\n",
                package.hardware_id);
    }

    printf("OTA3 version=%u.%u.%u.%u key_id=0x%08" PRIX32
           " A=%zu bytes B=%zu bytes\n",
           CAN_OTA_VERSION_MAJOR(package.firmware_version),
           CAN_OTA_VERSION_MINOR(package.firmware_version),
           CAN_OTA_VERSION_PATCH(package.firmware_version),
           CAN_OTA_VERSION_BUILD(package.firmware_version),
           package.key_id, package.image_sizes[0], package.image_sizes[1]);

    can_fd = open_can_socket(interface);
    if (can_fd < 0) {
        goto out;
    }
    if (enter_bootloader(can_fd, timeout_ms, &target_slot) < 0 ||
        send_authenticated_manifest(can_fd, &package, target_slot,
                                    timeout_ms, &resume_offset) < 0) {
        goto out;
    }
    printf("Transmitting Slot %s from offset %zu/%zu; "
           "confirmed slot is untouched.\n",
           slot_name(target_slot), resume_offset,
           package.image_sizes[target_slot]);
    if (send_image(can_fd, package.images[target_slot],
                   package.image_sizes[target_slot],
                   resume_offset,
                   pacing_us, timeout_ms) < 0 ||
        wait_app_confirmation(can_fd, package.firmware_version,
                              target_slot) < 0) {
        goto out;
    }

    printf("Signed resumable A/B CAN OTA completed and trial image confirmed.\n");
    result = 0;

out:
    if (can_fd >= 0) {
        close(can_fd);
    }
    free(file_data);
    return result;
}
