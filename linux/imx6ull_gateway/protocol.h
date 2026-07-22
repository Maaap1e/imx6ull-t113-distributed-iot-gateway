#ifndef BOARD_TCP_PROTOCOL_H
#define BOARD_TCP_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define FRAME_MAGIC 0x55AAu
#define FRAME_VERSION 1u
#define FRAME_HEADER_SIZE 20u
#define MAX_PAYLOAD_SIZE 4096u
#define PROTOCOL_RECV_TIMEOUT (-5)

typedef enum {
    FRAME_TYPE_DATA = 1,
    FRAME_TYPE_HEARTBEAT = 2
} frame_type_t;

typedef struct {
    uint8_t type;
    uint32_t seq;
    uint32_t timestamp;
    uint32_t payload_len;
    uint32_t crc32;
} frame_header_t;

int protocol_send_frame(int fd, uint8_t type, uint32_t seq,
                        const void *payload, uint32_t payload_len);

int protocol_recv_frame(int fd, frame_header_t *header,
                        uint8_t *payload, size_t payload_capacity);

uint32_t protocol_crc32(const void *data, size_t len);
const char *protocol_frame_type_name(uint8_t type);

#endif
