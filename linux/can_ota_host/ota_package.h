#ifndef OTA_PACKAGE_H
#define OTA_PACKAGE_H

#include <stddef.h>
#include <stdint.h>

#include "../../common/ota_contract.h"

typedef struct {
    uint8_t flags;
    uint16_t hardware_id;
    uint32_t firmware_version;
    const uint8_t *image;
    size_t image_size;
    uint32_t image_crc32;
} ota_package_t;

uint32_t ota_crc32_buffer(const uint8_t *data, size_t len);
int ota_package_parse(const uint8_t *data, size_t size,
                      ota_package_t *package,
                      char *error, size_t error_size);

#endif
