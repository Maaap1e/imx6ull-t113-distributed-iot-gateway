#include <errno.h>
#include <fcntl.h>
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
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "ota_package.h"

#define CAN_OTA_STATUS_READY    0x01u
#define CAN_OTA_STATUS_ERASING  0x02u
#define CAN_OTA_STATUS_WRITING  0x03u
#define CAN_OTA_STATUS_VERIFY   0x04u
#define CAN_OTA_STATUS_DONE     0x05u
#define CAN_OTA_STATUS_ERROR    0xE0u

#define DEFAULT_CAN_IFACE       "can0"
#define DEFAULT_PACING_US       20000u
#define DEFAULT_STATUS_TIMEOUT  5000
#define DATA_BYTES_PER_FRAME    6u
#define ENTER_ATTEMPTS          30
#define ENTER_PROBE_TIMEOUT_MS  200
#define APP_CONFIRM_TIMEOUT_MS  15000

typedef struct {
    uint8_t status;
    uint8_t error;
    uint8_t progress;
    uint16_t seq;
    uint16_t received_kb;
} ota_status_t;

static void usage(const char *prog)
{
    printf("Usage: %s -f stm32_app.ota [-i can0] [-p pacing_us] [-t timeout_ms]\n", prog);
    printf("Example: %s -i can0 -f stm32_dht11_app-v1.2.0.ota\n", prog);
}

static const char *status_name(uint8_t status)
{
    switch (status) {
    case CAN_OTA_STATUS_READY:
        return "ready";
    case CAN_OTA_STATUS_ERASING:
        return "erasing";
    case CAN_OTA_STATUS_WRITING:
        return "writing";
    case CAN_OTA_STATUS_VERIFY:
        return "verify";
    case CAN_OTA_STATUS_DONE:
        return "done";
    case CAN_OTA_STATUS_ERROR:
        return "error";
    default:
        return "unknown";
    }
}

static const char *error_name(uint8_t error)
{
    switch (error) {
    case 0x00:
        return "none";
    case 0x01:
        return "timeout";
    case 0x02:
        return "firmware-info";
    case 0x03:
        return "image-size";
    case 0x04:
        return "sequence";
    case 0x05:
        return "flash";
    case 0x06:
        return "image-crc";
    case 0x07:
        return "app-vector";
    case 0x08:
        return "manifest";
    case 0x09:
        return "hardware-id";
    case 0x0A:
        return "rollback-policy";
    case 0x0B:
        return "boot-metadata";
    default:
        return "unknown";
    }
}

static void put_le16(uint8_t *buf, uint16_t value)
{
    buf[0] = (uint8_t)(value & 0xFFu);
    buf[1] = (uint8_t)(value >> 8);
}

static void put_le32(uint8_t *buf, uint32_t value)
{
    buf[0] = (uint8_t)(value & 0xFFu);
    buf[1] = (uint8_t)((value >> 8) & 0xFFu);
    buf[2] = (uint8_t)((value >> 16) & 0xFFu);
    buf[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static uint16_t get_le16(const uint8_t *buf)
{
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static int64_t monotonic_ms(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
        perror("clock_gettime");
        return -1;
    }
    return ((int64_t)now.tv_sec * 1000) + (now.tv_nsec / 1000000);
}

static int load_file(const char *path, uint8_t **out_data, size_t *out_size)
{
    FILE *fp;
    long size;
    uint8_t *data;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        perror("fopen firmware");
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) < 0) {
        perror("fseek");
        fclose(fp);
        return -1;
    }

    size = ftell(fp);
    if (size <= 0) {
        fprintf(stderr, "invalid firmware size: %ld\n", size);
        fclose(fp);
        return -1;
    }

    rewind(fp);
    data = (uint8_t *)malloc((size_t)size);
    if (data == NULL) {
        fprintf(stderr, "out of memory\n");
        fclose(fp);
        return -1;
    }

    if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
        perror("fread firmware");
        free(data);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    *out_data = data;
    *out_size = (size_t)size;
    return 0;
}

static int open_can_socket(const char *iface)
{
    int fd;
    struct ifreq ifr;
    struct sockaddr_can addr;
    struct can_filter filters[] = {
        {
            CAN_OTA_ID_STATUS,
            CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG
        },
        {
            CAN_APP_ID_HEARTBEAT,
            CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG
        }
    };

    fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0) {
        perror("socket CAN");
        return -1;
    }

    if (setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER,
                   filters, sizeof(filters)) < 0) {
        perror("setsockopt CAN_RAW_FILTER");
        close(fd);
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", iface);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl SIOCGIFINDEX");
        close(fd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind CAN");
        close(fd);
        return -1;
    }

    return fd;
}

static int send_can_frame(int fd, uint32_t id, const uint8_t *data, uint8_t len)
{
    struct can_frame frame;

    if (len > 8) {
        errno = EINVAL;
        return -1;
    }

    memset(&frame, 0, sizeof(frame));
    frame.can_id = id;
    frame.can_dlc = len;
    if (len > 0 && data != NULL) {
        memcpy(frame.data, data, len);
    }

    if (write(fd, &frame, sizeof(frame)) != (ssize_t)sizeof(frame)) {
        perror("write CAN");
        return -1;
    }

    return 0;
}

static int recv_status(int fd, ota_status_t *status, int timeout_ms)
{
    fd_set rfds;
    struct timeval tv;

    while (1) {
        int ret;
        struct can_frame frame;

        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        ret = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (ret == 0) {
            return 0;
        }
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("select");
            return -1;
        }

        ret = (int)read(fd, &frame, sizeof(frame));
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("read CAN");
            return -1;
        }
        if ((size_t)ret != sizeof(frame)) {
            continue;
        }
        if ((frame.can_id & CAN_EFF_FLAG) || (frame.can_id & CAN_RTR_FLAG)) {
            continue;
        }
        if ((frame.can_id & CAN_SFF_MASK) != CAN_OTA_ID_STATUS || frame.can_dlc < 7) {
            continue;
        }

        status->status = frame.data[0];
        status->error = frame.data[1];
        status->progress = frame.data[2];
        status->seq = get_le16(&frame.data[3]);
        status->received_kb = get_le16(&frame.data[5]);
        return 1;
    }
}

static int wait_status(int fd, ota_status_t *status, int timeout_ms)
{
    int ret = recv_status(fd, status, timeout_ms);

    if (ret == 0) {
        fprintf(stderr, "timeout waiting STM32 OTA status\n");
        return -1;
    }
    if (ret < 0) {
        return -1;
    }

    printf("STM32 status=%s(0x%02X) error=%s(0x%02X) progress=%u%% seq=%u received=%uKB\n",
           status_name(status->status),
           status->status,
           error_name(status->error),
           status->error,
           status->progress,
           status->seq,
           status->received_kb);

    if (status->status == CAN_OTA_STATUS_ERROR) {
        fprintf(stderr, "STM32 reported OTA error: %s (0x%02X)\n",
                error_name(status->error), status->error);
        return -1;
    }

    return 0;
}

static int send_enter(int fd, int timeout_ms)
{
    uint8_t data[8] = { CAN_OTA_CMD_ENTER, 0, 0, 0, 0, 0, 0, 0 };
    uint8_t app_control[8] = {
        CAN_APP_CMD_ENTER_BOOT, 0, 0, 0, 0, 0, 0,
        CAN_APP_CMD_ENTER_BOOT
    };
    ota_status_t status;
    int attempt;
    int probe_timeout = timeout_ms < ENTER_PROBE_TIMEOUT_MS ?
                        timeout_ms :
                        ENTER_PROBE_TIMEOUT_MS;

    if (probe_timeout <= 0) {
        probe_timeout = ENTER_PROBE_TIMEOUT_MS;
    }

    /*
     * If the App is currently running, request a controlled reset through
     * its normal command ID first. A bootloader already in recovery simply
     * ignores this frame. Repeated 0x300 probes below then cover the reset
     * and CAN reinitialization window without requiring a manual reset.
     */
    printf("Requesting running App to enter Bootloader...\n");
    if (send_can_frame(fd, CAN_APP_ID_CONTROL,
                       app_control, sizeof(app_control)) < 0) {
        return -1;
    }
    usleep(100000u);

    for (attempt = 0; attempt < ENTER_ATTEMPTS; attempt++) {
        printf("Sending ENTER OTA attempt %d...\n", attempt + 1);
        if (send_can_frame(fd, CAN_OTA_ID_ENTER, data, 8) < 0) {
            return -1;
        }
        if (recv_status(fd, &status, probe_timeout) > 0) {
            printf("STM32 status=%s(0x%02X) error=0x%02X progress=%u%%\n",
                   status_name(status.status),
                   status.status,
                   status.error,
                   status.progress);
            if (status.status == CAN_OTA_STATUS_ERROR) {
                return -1;
            }
            if (status.status == CAN_OTA_STATUS_READY) {
                return 0;
            }
        }
    }

    fprintf(stderr, "STM32 did not enter OTA mode\n");
    return -1;
}

static int send_manifest(int fd, const ota_package_t *package,
                         int timeout_ms)
{
    uint8_t data[8];
    ota_status_t status;

    data[0] = CAN_OTA_PROTOCOL_VERSION;
    data[1] = package->flags;
    put_le16(&data[2], package->hardware_id);
    put_le32(&data[4], package->firmware_version);

    printf("Sending manifest: hardware=0x%04X version=%u.%u.%u.%u flags=0x%02X\n",
           package->hardware_id,
           CAN_OTA_VERSION_MAJOR(package->firmware_version),
           CAN_OTA_VERSION_MINOR(package->firmware_version),
           CAN_OTA_VERSION_PATCH(package->firmware_version),
           CAN_OTA_VERSION_BUILD(package->firmware_version),
           package->flags);

    if (send_can_frame(fd, CAN_OTA_ID_MANIFEST, data, 8) < 0) {
        return -1;
    }
    if (wait_status(fd, &status, timeout_ms) < 0) {
        return -1;
    }
    if (status.status != CAN_OTA_STATUS_READY) {
        fprintf(stderr, "unexpected manifest response: 0x%02X\n",
                status.status);
        return -1;
    }
    return 0;
}

static int send_info(int fd, size_t size, uint32_t crc, int timeout_ms)
{
    uint8_t data[8];
    ota_status_t status;

    put_le32(&data[0], (uint32_t)size);
    put_le32(&data[4], crc);

    printf("Sending firmware info: size=%lu crc32=0x%08X\n",
           (unsigned long)size,
           crc);

    if (send_can_frame(fd, CAN_OTA_ID_INFO, data, 8) < 0) {
        return -1;
    }

    while (1) {
        if (wait_status(fd, &status, timeout_ms) < 0) {
            return -1;
        }
        if (status.status == CAN_OTA_STATUS_WRITING) {
            break;
        }
        if (status.status != CAN_OTA_STATUS_ERASING &&
            status.status != CAN_OTA_STATUS_READY) {
            fprintf(stderr, "unexpected STM32 status before data transfer: 0x%02X\n",
                    status.status);
            return -1;
        }
    }

    return 0;
}

static int send_firmware(int fd, const uint8_t *fw, size_t size,
                         unsigned int pacing_us, int timeout_ms)
{
    size_t offset = 0;
    uint16_t seq = 0;
    ota_status_t status;

    while (offset < size) {
        uint8_t data[8];
        size_t remain = size - offset;
        size_t chunk = remain >= DATA_BYTES_PER_FRAME ? DATA_BYTES_PER_FRAME : remain;

        memset(data, 0xFF, sizeof(data));
        put_le16(&data[0], seq);
        memcpy(&data[2], fw + offset, chunk);

        if (send_can_frame(fd, CAN_OTA_ID_DATA, data, 8) < 0) {
            return -1;
        }

        offset += chunk;

        if (pacing_us > 0) {
            usleep(pacing_us);
        }

        if (seq == 0 || (seq % 32u) == 0u || offset == size) {
            if (wait_status(fd, &status, timeout_ms) < 0) {
                return -1;
            }
        }

        seq++;
    }

    while (1) {
        if (wait_status(fd, &status, timeout_ms) < 0) {
            return -1;
        }
        if (status.status == CAN_OTA_STATUS_DONE) {
            break;
        }
    }

    return 0;
}

static int wait_app_confirmation(int fd, uint32_t firmware_version)
{
    uint8_t expected_major = CAN_OTA_VERSION_MAJOR(firmware_version);
    uint8_t expected_minor = CAN_OTA_VERSION_MINOR(firmware_version);
    uint8_t expected_patch = CAN_OTA_VERSION_PATCH(firmware_version);
    uint8_t expected_build = CAN_OTA_VERSION_BUILD(firmware_version);
    int64_t start_ms = monotonic_ms();
    int64_t deadline_ms;

    if (start_ms < 0) {
        return -1;
    }
    deadline_ms = start_ms + APP_CONFIRM_TIMEOUT_MS;

    while (1) {
        fd_set rfds;
        struct timeval tv;
        struct can_frame frame;
        int64_t now_ms;
        int64_t remaining_ms;
        int ret;
        uint8_t checksum = 0u;
        int i;

        now_ms = monotonic_ms();
        if (now_ms < 0) {
            return -1;
        }
        remaining_ms = deadline_ms - now_ms;
        if (remaining_ms <= 0) {
            fprintf(stderr,
                    "timeout waiting for confirmed STM32 App heartbeat\n");
            return -1;
        }

        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = (time_t)(remaining_ms / 1000);
        tv.tv_usec = (suseconds_t)((remaining_ms % 1000) * 1000);
        ret = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (ret == 0) {
            fprintf(stderr,
                    "timeout waiting for confirmed STM32 App heartbeat\n");
            return -1;
        }
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("select App heartbeat");
            return -1;
        }
        ret = (int)read(fd, &frame, sizeof(frame));
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("read App heartbeat");
            return -1;
        }
        if ((size_t)ret != sizeof(frame) ||
            (frame.can_id & (CAN_EFF_FLAG | CAN_RTR_FLAG)) != 0u ||
            (frame.can_id & CAN_SFF_MASK) != CAN_APP_ID_HEARTBEAT ||
            frame.can_dlc != 8u) {
            continue;
        }
        for (i = 0; i < 7; i++) {
            checksum = (uint8_t)(checksum + frame.data[i]);
        }
        if (checksum != frame.data[7]) {
            fprintf(stderr, "ignoring heartbeat with invalid checksum\n");
            continue;
        }
        if (frame.data[0] != expected_major ||
            frame.data[1] != expected_minor ||
            frame.data[4] != expected_patch ||
            frame.data[5] != expected_build) {
            fprintf(stderr,
                    "received App heartbeat version %u.%u.%u.%u, "
                    "expected %u.%u.%u.%u\n",
                    frame.data[0], frame.data[1],
                    frame.data[4], frame.data[5],
                    expected_major, expected_minor,
                    expected_patch, expected_build);
            continue;
        }

        printf("STM32 App confirmed and heartbeat version "
               "%u.%u.%u.%u received.\n",
               frame.data[0], frame.data[1],
               frame.data[4], frame.data[5]);
        return 0;
    }
}

int main(int argc, char *argv[])
{
    const char *iface = DEFAULT_CAN_IFACE;
    const char *fw_path = NULL;
    unsigned int pacing_us = DEFAULT_PACING_US;
    int timeout_ms = DEFAULT_STATUS_TIMEOUT;
    uint8_t *package_data = NULL;
    size_t package_size = 0;
    ota_package_t package;
    char package_error[160];
    int can_fd;
    int opt;
    int ret = 1;

    while ((opt = getopt(argc, argv, "i:f:p:t:h")) != -1) {
        switch (opt) {
        case 'i':
            iface = optarg;
            break;
        case 'f':
            fw_path = optarg;
            break;
        case 'p':
            pacing_us = (unsigned int)strtoul(optarg, NULL, 10);
            break;
        case 't':
            timeout_ms = atoi(optarg);
            break;
        case 'h':
        default:
            usage(argv[0]);
            return opt == 'h' ? 0 : 1;
        }
    }

    if (fw_path == NULL) {
        usage(argv[0]);
        return 1;
    }

    if (load_file(fw_path, &package_data, &package_size) < 0) {
        return 1;
    }
    if (ota_package_parse(package_data, package_size, &package,
                          package_error, sizeof(package_error)) < 0) {
        fprintf(stderr, "invalid OTA package: %s\n", package_error);
        free(package_data);
        return 1;
    }
    if (package.hardware_id != CAN_OTA_HARDWARE_ID_STM32F103) {
        fprintf(stderr,
                "package hardware 0x%04X does not match STM32F103 target 0x%04X\n",
                package.hardware_id, CAN_OTA_HARDWARE_ID_STM32F103);
        free(package_data);
        return 1;
    }

    can_fd = open_can_socket(iface);
    if (can_fd < 0) {
        free(package_data);
        return 1;
    }

    printf("CAN iface=%s package=%s image_size=%lu crc32=0x%08X pacing=%uus\n",
           iface,
           fw_path,
           (unsigned long)package.image_size,
           package.image_crc32,
           pacing_us);

    if (send_enter(can_fd, timeout_ms) < 0) {
        goto out;
    }
    if (send_manifest(can_fd, &package, timeout_ms) < 0) {
        goto out;
    }
    if (send_info(can_fd, package.image_size,
                  package.image_crc32, timeout_ms) < 0) {
        goto out;
    }
    if (send_firmware(can_fd, package.image, package.image_size,
                      pacing_us, timeout_ms) < 0) {
        goto out;
    }
    if (wait_app_confirmation(can_fd, package.firmware_version) < 0) {
        goto out;
    }

    printf("STM32 CAN OTA and trial-boot confirmation finished successfully.\n");
    ret = 0;

out:
    close(can_fd);
    free(package_data);
    return ret;
}
