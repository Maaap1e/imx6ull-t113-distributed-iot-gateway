#ifndef OTA_SHA256_H
#define OTA_SHA256_H

#include <stddef.h>
#include <stdint.h>

#define OTA_SHA256_DIGEST_SIZE 32u

typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t block[64];
    size_t block_used;
} ota_sha256_ctx_t;

void ota_sha256_init(ota_sha256_ctx_t *ctx);
void ota_sha256_update(ota_sha256_ctx_t *ctx, const void *data, size_t len);
void ota_sha256_final(ota_sha256_ctx_t *ctx,
                      uint8_t digest[OTA_SHA256_DIGEST_SIZE]);
void ota_sha256(const void *data, size_t len,
                uint8_t digest[OTA_SHA256_DIGEST_SIZE]);

#endif
