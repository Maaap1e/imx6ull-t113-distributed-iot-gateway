#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

static int read_full(int fd, void *buf, size_t len)
{
    uint8_t *p = (uint8_t *)buf;
    size_t off = 0;

    while (off < len) {
        ssize_t n = recv(fd, p + off, len - off, 0);
        if (n == 0) {
            return 0;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("recv");
            return -1;
        }
        off += (size_t)n;
    }

    return 1;
}

static int write_full(int fd, const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    size_t off = 0;

    while (off < len) {
        ssize_t n = send(fd, p + off, len - off, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("send");
            return -1;
        }
        if (n == 0) {
            errno = EPIPE;
            return -1;
        }
        off += (size_t)n;
    }

    return 0;
}

uint32_t protocol_crc32(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;

    for (i = 0; i < len; ++i) {
        int bit;
        crc ^= p[i];
        for (bit = 0; bit < 8; ++bit) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }

    return ~crc;
}

int protocol_send_frame(int fd, uint8_t type, uint32_t seq,
                        const void *payload, uint32_t payload_len)
{
    uint8_t header[FRAME_HEADER_SIZE];
    uint16_t magic_n = htons((uint16_t)FRAME_MAGIC);
    uint32_t seq_n = htonl(seq);
    uint32_t timestamp_n = htonl((uint32_t)time(NULL));
    uint32_t len_n = htonl(payload_len);
    uint32_t crc;
    uint32_t crc_n;

    if (payload_len > MAX_PAYLOAD_SIZE) {
        errno = EMSGSIZE;
        return -1;
    }
    if (payload_len > 0 && payload == NULL) {
        errno = EINVAL;
        return -1;
    }

    crc = protocol_crc32(payload, payload_len);
    crc_n = htonl(crc);

    memcpy(header + 0, &magic_n, sizeof(magic_n));
    header[2] = FRAME_VERSION;
    header[3] = type;
    memcpy(header + 4, &seq_n, sizeof(seq_n));
    memcpy(header + 8, &timestamp_n, sizeof(timestamp_n));
    memcpy(header + 12, &len_n, sizeof(len_n));
    memcpy(header + 16, &crc_n, sizeof(crc_n));

    if (write_full(fd, header, sizeof(header)) < 0) {
        return -1;
    }

    if (payload_len > 0 && write_full(fd, payload, payload_len) < 0) {
        return -1;
    }

    return 0;
}

int protocol_recv_frame(int fd, frame_header_t *header,
                        uint8_t *payload, size_t payload_capacity)
{
    uint8_t raw[FRAME_HEADER_SIZE];
    uint16_t magic_n;
    uint32_t tmp_n;
    uint32_t actual_crc;
    int ret;

    ret = read_full(fd, raw, sizeof(raw));
    if (ret <= 0) {
        return ret;
    }

    memcpy(&magic_n, raw + 0, sizeof(magic_n));
    if (ntohs(magic_n) != FRAME_MAGIC || raw[2] != FRAME_VERSION) {
        fprintf(stderr, "protocol error: bad header magic=0x%04X version=%u\n",
                ntohs(magic_n), raw[2]);
        return -2;
    }

    memset(header, 0, sizeof(*header));
    header->type = raw[3];

    memcpy(&tmp_n, raw + 4, sizeof(tmp_n));
    header->seq = ntohl(tmp_n);
    memcpy(&tmp_n, raw + 8, sizeof(tmp_n));
    header->timestamp = ntohl(tmp_n);
    memcpy(&tmp_n, raw + 12, sizeof(tmp_n));
    header->payload_len = ntohl(tmp_n);
    memcpy(&tmp_n, raw + 16, sizeof(tmp_n));
    header->crc32 = ntohl(tmp_n);

    if (header->payload_len > MAX_PAYLOAD_SIZE ||
        header->payload_len > payload_capacity) {
        fprintf(stderr, "protocol error: payload too large len=%u capacity=%lu\n",
                header->payload_len, (unsigned long)payload_capacity);
        return -3;
    }

    if (header->payload_len > 0) {
        ret = read_full(fd, payload, header->payload_len);
        if (ret <= 0) {
            return ret;
        }
    }

    actual_crc = protocol_crc32(payload, header->payload_len);
    if (actual_crc != header->crc32) {
        fprintf(stderr, "protocol error: crc mismatch seq=%u expected=0x%08X actual=0x%08X len=%u\n",
                header->seq, header->crc32, actual_crc, header->payload_len);
        return -4;
    }

    return 1;
}

const char *protocol_frame_type_name(uint8_t type)
{
    switch (type) {
    case FRAME_TYPE_DATA:
        return "data";
    case FRAME_TYPE_HEARTBEAT:
        return "heartbeat";
    default:
        return "unknown";
    }
}
