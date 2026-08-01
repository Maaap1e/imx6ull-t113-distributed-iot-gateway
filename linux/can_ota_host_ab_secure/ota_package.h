#ifndef OTA_AB_PACKAGE_H
#define OTA_AB_PACKAGE_H

#include <stddef.h>
#include <stdint.h>
#include "../../common/ota_contract_ab.h"

typedef struct {
    uint8_t flags;
    uint16_t hardware_id;
    uint32_t firmware_version;
    uint32_t key_id;
    const uint8_t *header;
    const uint8_t *images[2];
    const uint8_t *block_hash_tables[2];
    size_t image_sizes[2];
    uint16_t block_counts[2];
    uint32_t image_crc32[2];
    uint8_t image_sha256[2][CAN_OTA_AB_SHA256_SIZE];
    const uint8_t *signature;
} ota_ab_package_t;

uint32_t ota_ab_package_crc32(const uint8_t *data, size_t len);
int ota_ab_package_parse(const uint8_t *data, size_t size,
                         ota_ab_package_t *package,
                         char *error, size_t error_size);

#endif
