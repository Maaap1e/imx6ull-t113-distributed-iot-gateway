#include "ota_package.h"

#include <stdio.h>
#include <string.h>

static uint16_t get_le16(const uint8_t *buf)
{
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static uint32_t get_le32(const uint8_t *buf)
{
    return (uint32_t)buf[0] |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}

uint32_t ota_crc32_buffer(const uint8_t *data, size_t len)
{
    uint32_t crc = 0u;
    size_t i;

    crc = ~crc;
    for (i = 0u; i < len; i++) {
        int bit;

        crc ^= data[i];
        for (bit = 0; bit < 8; bit++) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static int fail(char *error, size_t error_size, const char *message)
{
    if (error != NULL && error_size > 0u) {
        snprintf(error, error_size, "%s", message);
    }
    return -1;
}

int ota_package_parse(const uint8_t *data, size_t size,
                      ota_package_t *package,
                      char *error, size_t error_size)
{
    uint16_t header_size;
    uint16_t hardware_id;
    uint32_t firmware_version;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t header_crc32;
    uint32_t initial_sp;
    uint32_t reset_handler;
    uint32_t reset_address;

    if (data == NULL || package == NULL) {
        return fail(error, error_size, "invalid parser argument");
    }
    if (size < CAN_OTA_PACKAGE_HEADER_SIZE) {
        return fail(error, error_size, "file is smaller than OTA header");
    }
    if (get_le32(data + CAN_OTA_PACKAGE_OFF_MAGIC) !=
        CAN_OTA_PACKAGE_MAGIC) {
        return fail(error, error_size,
                    "invalid OTA magic (raw .bin files are not accepted)");
    }

    header_size = get_le16(data + CAN_OTA_PACKAGE_OFF_HEADER_SIZE);
    if (header_size != CAN_OTA_PACKAGE_HEADER_SIZE ||
        data[CAN_OTA_PACKAGE_OFF_FORMAT] !=
            CAN_OTA_PACKAGE_FORMAT_VERSION) {
        return fail(error, error_size, "unsupported OTA package format");
    }
    if ((data[CAN_OTA_PACKAGE_OFF_FLAGS] &
         (uint8_t)~CAN_OTA_MANIFEST_KNOWN_FLAGS) != 0u) {
        return fail(error, error_size, "unknown OTA package flags");
    }
    if (get_le16(data + CAN_OTA_PACKAGE_OFF_RESERVED0) != 0u ||
        get_le32(data + CAN_OTA_PACKAGE_OFF_RESERVED1) != 0u) {
        return fail(error, error_size, "reserved OTA header field is nonzero");
    }

    header_crc32 = get_le32(data + CAN_OTA_PACKAGE_OFF_HEADER_CRC32);
    if (ota_crc32_buffer(data, CAN_OTA_PACKAGE_CRC_BYTES) !=
        header_crc32) {
        return fail(error, error_size, "OTA header CRC32 mismatch");
    }

    hardware_id = get_le16(data + CAN_OTA_PACKAGE_OFF_HARDWARE_ID);
    firmware_version = get_le32(data + CAN_OTA_PACKAGE_OFF_VERSION);
    image_size = get_le32(data + CAN_OTA_PACKAGE_OFF_IMAGE_SIZE);
    image_crc32 = get_le32(data + CAN_OTA_PACKAGE_OFF_IMAGE_CRC32);

    if (hardware_id == 0u || firmware_version == 0u) {
        return fail(error, error_size, "missing hardware ID or version");
    }
    if (image_size < 8u || image_size > CAN_OTA_APP_MAX_SIZE) {
        return fail(error, error_size,
                    "invalid image size for STM32 App slot");
    }
    if ((size_t)header_size + (size_t)image_size != size) {
        return fail(error, error_size, "OTA file length does not match header");
    }
    if (ota_crc32_buffer(data + header_size, image_size) != image_crc32) {
        return fail(error, error_size, "firmware image CRC32 mismatch");
    }

    initial_sp = get_le32(data + header_size);
    reset_handler = get_le32(data + header_size + 4u);
    reset_address = reset_handler & ~1u;
    if (initial_sp < CAN_OTA_SRAM_START ||
        initial_sp > CAN_OTA_SRAM_END ||
        (reset_handler & 1u) == 0u ||
        reset_address < CAN_OTA_APP_ADDRESS ||
        reset_address >= CAN_OTA_APP_ADDRESS + image_size) {
        return fail(error, error_size, "invalid STM32 App vector table");
    }

    memset(package, 0, sizeof(*package));
    package->flags = data[CAN_OTA_PACKAGE_OFF_FLAGS];
    package->hardware_id = hardware_id;
    package->firmware_version = firmware_version;
    package->image = data + header_size;
    package->image_size = image_size;
    package->image_crc32 = image_crc32;
    return 0;
}
