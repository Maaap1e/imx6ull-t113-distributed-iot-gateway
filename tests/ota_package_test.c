#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../linux/can_ota_host/ota_package.h"

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

static size_t make_package(uint8_t *data, size_t capacity)
{
    static const uint8_t image[] = {
        0x00, 0x00, 0x01, 0x20, 0x09, 0x00, 0x01, 0x08,
        0x11, 0x22, 0x33, 0x44, 0x55
    };
    uint32_t image_crc;
    uint32_t header_crc;
    size_t total = CAN_OTA_PACKAGE_HEADER_SIZE + sizeof(image);

    assert(capacity >= total);
    memset(data, 0, total);
    put_le32(data + CAN_OTA_PACKAGE_OFF_MAGIC, CAN_OTA_PACKAGE_MAGIC);
    put_le16(data + CAN_OTA_PACKAGE_OFF_HEADER_SIZE,
             CAN_OTA_PACKAGE_HEADER_SIZE);
    data[CAN_OTA_PACKAGE_OFF_FORMAT] = CAN_OTA_PACKAGE_FORMAT_VERSION;
    put_le16(data + CAN_OTA_PACKAGE_OFF_HARDWARE_ID,
             CAN_OTA_HARDWARE_ID_STM32F103);
    put_le32(data + CAN_OTA_PACKAGE_OFF_VERSION,
             CAN_OTA_VERSION(1, 2, 0, 0));
    put_le32(data + CAN_OTA_PACKAGE_OFF_IMAGE_SIZE, sizeof(image));

    image_crc = ota_crc32_buffer(image, sizeof(image));
    put_le32(data + CAN_OTA_PACKAGE_OFF_IMAGE_CRC32, image_crc);
    header_crc = ota_crc32_buffer(data, CAN_OTA_PACKAGE_CRC_BYTES);
    put_le32(data + CAN_OTA_PACKAGE_OFF_HEADER_CRC32, header_crc);
    memcpy(data + CAN_OTA_PACKAGE_HEADER_SIZE, image, sizeof(image));
    return total;
}

static void refresh_header_crc(uint8_t *data)
{
    put_le32(data + CAN_OTA_PACKAGE_OFF_HEADER_CRC32,
             ota_crc32_buffer(data, CAN_OTA_PACKAGE_CRC_BYTES));
}

static void refresh_image_and_header_crc(uint8_t *data, size_t size)
{
    size_t image_size = size - CAN_OTA_PACKAGE_HEADER_SIZE;

    put_le32(data + CAN_OTA_PACKAGE_OFF_IMAGE_CRC32,
             ota_crc32_buffer(data + CAN_OTA_PACKAGE_HEADER_SIZE,
                              image_size));
    refresh_header_crc(data);
}

int main(void)
{
    uint8_t data[128];
    uint8_t copy[128];
    ota_package_t package;
    char error[160];
    size_t size = make_package(data, sizeof(data));

    assert(ota_package_parse(data, size, &package,
                             error, sizeof(error)) == 0);
    assert(package.hardware_id == CAN_OTA_HARDWARE_ID_STM32F103);
    assert(package.firmware_version == CAN_OTA_VERSION(1, 2, 0, 0));
    assert(package.image_size == size - CAN_OTA_PACKAGE_HEADER_SIZE);
    assert(package.image == data + CAN_OTA_PACKAGE_HEADER_SIZE);

    memcpy(copy, data, size);
    copy[CAN_OTA_PACKAGE_OFF_HEADER_CRC32] ^= 0x01u;
    assert(ota_package_parse(copy, size, &package,
                             error, sizeof(error)) < 0);
    assert(strstr(error, "header CRC32") != NULL);

    memcpy(copy, data, size);
    copy[size - 1u] ^= 0x01u;
    assert(ota_package_parse(copy, size, &package,
                             error, sizeof(error)) < 0);
    assert(strstr(error, "image CRC32") != NULL);

    assert(ota_package_parse(data, size - 1u, &package,
                             error, sizeof(error)) < 0);
    assert(strstr(error, "length") != NULL);

    memcpy(copy, data, size);
    copy[CAN_OTA_PACKAGE_OFF_FLAGS] = 0x80u;
    refresh_header_crc(copy);
    assert(ota_package_parse(copy, size, &package,
                             error, sizeof(error)) < 0);
    assert(strstr(error, "flags") != NULL);

    memcpy(copy, data, size);
    put_le32(copy + CAN_OTA_PACKAGE_OFF_MAGIC, 0xFFFFFFFFu);
    refresh_header_crc(copy);
    assert(ota_package_parse(copy, size, &package,
                             error, sizeof(error)) < 0);
    assert(strstr(error, "raw .bin") != NULL);

    memcpy(copy, data, size);
    put_le32(copy + CAN_OTA_PACKAGE_HEADER_SIZE + 4u, 0x08000001u);
    refresh_image_and_header_crc(copy, size);
    assert(ota_package_parse(copy, size, &package,
                             error, sizeof(error)) < 0);
    assert(strstr(error, "vector") != NULL);

    puts("OTA package tests passed");
    return 0;
}
