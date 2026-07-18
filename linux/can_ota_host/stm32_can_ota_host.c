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
#include <time.h>
#include <unistd.h>

#define CAN_OTA_ID_ENTER        0x300u
#define CAN_OTA_ID_INFO         0x301u
#define CAN_OTA_ID_DATA         0x302u
#define CAN_OTA_ID_STATUS       0x380u

#define CAN_OTA_CMD_ENTER       0xA5u

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

typedef struct {
    uint8_t status;
    uint8_t error;
    uint8_t progress;
    uint16_t seq;
    uint16_t received_kb;
} ota_status_t;

static void usage(const char *prog)
{
    printf("Usage: %s -f stm32_app.bin [-i can0] [-p pacing_us] [-t timeout_ms]\n", prog);
    printf("Example: %s -i can0 -f stm32_dht11_app.bin\n", prog);
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

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    size_t i;

    crc = ~crc;
    for (i = 0; i < len; i++) {
        int bit;
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }

    return ~crc;
}

static uint32_t crc32_buffer(const uint8_t *data, size_t len)
{
    return crc32_update(0, data, len);
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
    struct can_filter filter = {
        CAN_OTA_ID_STATUS,
        CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG
    };

    fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0) {
        perror("socket CAN");
        return -1;
    }

    if (setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER,
                   &filter, sizeof(filter)) < 0) {
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

    printf("STM32 status=%s(0x%02X) error=0x%02X progress=%u%% seq=%u received=%uKB\n",
           status_name(status->status),
           status->status,
           status->error,
           status->progress,
           status->seq,
           status->received_kb);

    if (status->status == CAN_OTA_STATUS_ERROR) {
        fprintf(stderr, "STM32 reported OTA error: 0x%02X\n", status->error);
        return -1;
    }

    return 0;
}

static int send_enter(int fd, int timeout_ms)
{
    uint8_t data[8] = { CAN_OTA_CMD_ENTER, 0, 0, 0, 0, 0, 0, 0 };
    ota_status_t status;
    int attempt;
    int probe_timeout = timeout_ms < ENTER_PROBE_TIMEOUT_MS ?
                        timeout_ms :
                        ENTER_PROBE_TIMEOUT_MS;

    if (probe_timeout <= 0) {
        probe_timeout = ENTER_PROBE_TIMEOUT_MS;
    }

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
            return status.status == CAN_OTA_STATUS_ERROR ? -1 : 0;
        }
    }

    fprintf(stderr, "STM32 did not enter OTA mode\n");
    return -1;
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

int main(int argc, char *argv[])
{
    const char *iface = DEFAULT_CAN_IFACE;
    const char *fw_path = NULL;
    unsigned int pacing_us = DEFAULT_PACING_US;
    int timeout_ms = DEFAULT_STATUS_TIMEOUT;
    uint8_t *fw = NULL;
    size_t fw_size = 0;
    uint32_t crc;
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

    if (load_file(fw_path, &fw, &fw_size) < 0) {
        return 1;
    }

    crc = crc32_buffer(fw, fw_size);
    can_fd = open_can_socket(iface);
    if (can_fd < 0) {
        free(fw);
        return 1;
    }

    printf("CAN iface=%s firmware=%s size=%lu crc32=0x%08X pacing=%uus\n",
           iface,
           fw_path,
           (unsigned long)fw_size,
           crc,
           pacing_us);

    if (send_enter(can_fd, timeout_ms) < 0) {
        goto out;
    }
    if (send_info(can_fd, fw_size, crc, timeout_ms) < 0) {
        goto out;
    }
    if (send_firmware(can_fd, fw, fw_size, pacing_us, timeout_ms) < 0) {
        goto out;
    }

    printf("STM32 CAN OTA finished successfully.\n");
    ret = 0;

out:
    close(can_fd);
    free(fw);
    return ret;
}
