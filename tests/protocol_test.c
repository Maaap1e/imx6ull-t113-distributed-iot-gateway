#include "protocol.h"

#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static int failures;

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        failures++; \
    } \
} while (0)

static void make_header(uint8_t raw[FRAME_HEADER_SIZE], uint8_t type,
                        uint32_t seq, uint32_t len, uint32_t crc)
{
    uint16_t magic = htons(FRAME_MAGIC);
    uint32_t value;

    memset(raw, 0, FRAME_HEADER_SIZE);
    memcpy(raw, &magic, sizeof(magic));
    raw[2] = FRAME_VERSION;
    raw[3] = type;
    value = htonl(seq); memcpy(raw + 4, &value, sizeof(value));
    value = htonl((uint32_t)time(NULL)); memcpy(raw + 8, &value, sizeof(value));
    value = htonl(len); memcpy(raw + 12, &value, sizeof(value));
    value = htonl(crc); memcpy(raw + 16, &value, sizeof(value));
}

static void test_crc(void)
{
    CHECK(protocol_crc32("123456789", 9) == 0xCBF43926u);
}

static void test_round_trip(void)
{
    int fd[2];
    const char payload[] = "{\"ok\":true}";
    uint8_t received[128] = {0};
    frame_header_t header;

    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
    CHECK(protocol_send_frame(fd[0], FRAME_TYPE_DATA, 42, payload,
                              (uint32_t)strlen(payload)) == 0);
    CHECK(protocol_recv_frame(fd[1], &header, received, sizeof(received)) == 1);
    CHECK(header.type == FRAME_TYPE_DATA);
    CHECK(header.seq == 42);
    CHECK(header.payload_len == (uint32_t)strlen(payload));
    CHECK(memcmp(received, payload, strlen(payload)) == 0);
    close(fd[0]); close(fd[1]);
}

static void test_fragmented_and_coalesced(void)
{
    int fd[2];
    const uint8_t first[] = "first";
    const uint8_t second[] = "second";
    uint8_t raw[FRAME_HEADER_SIZE];
    uint8_t received[32] = {0};
    frame_header_t header;

    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
    make_header(raw, FRAME_TYPE_DATA, 7, sizeof(first) - 1,
                protocol_crc32(first, sizeof(first) - 1));
    CHECK(write(fd[0], raw, 3) == 3);
    CHECK(write(fd[0], raw + 3, 8) == 8);
    CHECK(write(fd[0], raw + 11, FRAME_HEADER_SIZE - 11) == (ssize_t)(FRAME_HEADER_SIZE - 11));
    CHECK(write(fd[0], first, sizeof(first) - 1) == (ssize_t)(sizeof(first) - 1));
    CHECK(protocol_send_frame(fd[0], FRAME_TYPE_HEARTBEAT, 8,
                              second, sizeof(second) - 1) == 0);

    CHECK(protocol_recv_frame(fd[1], &header, received, sizeof(received)) == 1);
    CHECK(header.seq == 7 && memcmp(received, first, sizeof(first) - 1) == 0);
    memset(received, 0, sizeof(received));
    CHECK(protocol_recv_frame(fd[1], &header, received, sizeof(received)) == 1);
    CHECK(header.seq == 8 && memcmp(received, second, sizeof(second) - 1) == 0);
    close(fd[0]); close(fd[1]);
}

static void test_rejects_corruption(void)
{
    int fd[2];
    uint8_t raw[FRAME_HEADER_SIZE];
    uint8_t payload[8] = {0};
    frame_header_t header;

    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
    make_header(raw, FRAME_TYPE_DATA, 9, 3, 0xDEADBEEFu);
    CHECK(write(fd[0], raw, sizeof(raw)) == (ssize_t)sizeof(raw));
    CHECK(write(fd[0], "bad", 3) == 3);
    CHECK(protocol_recv_frame(fd[1], &header, payload, sizeof(payload)) == -4);
    close(fd[0]); close(fd[1]);

    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
    make_header(raw, FRAME_TYPE_DATA, 10, MAX_PAYLOAD_SIZE + 1, 0);
    CHECK(write(fd[0], raw, sizeof(raw)) == (ssize_t)sizeof(raw));
    CHECK(protocol_recv_frame(fd[1], &header, payload, sizeof(payload)) == -3);
    close(fd[0]); close(fd[1]);
}

int main(void)
{
    test_crc();
    test_round_trip();
    test_fragmented_and_coalesced();
    test_rejects_corruption();
    if (failures != 0) {
        fprintf(stderr, "%d protocol test(s) failed\n", failures);
        return 1;
    }
    puts("protocol tests passed");
    return 0;
}
