#include "ota_package.h"

#include <stdio.h>
#include <string.h>
#include "../../common/crypto/sha256.h"

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

uint32_t ota_ab_package_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t index;
    for (index = 0u; index < len; index++) {
        int bit;
        crc ^= data[index];
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

static int validate_vector(const uint8_t *image, size_t image_size,
                           uint32_t slot_address)
{
    uint32_t sp = get_le32(image);
    uint32_t reset = get_le32(image + 4u);
    uint32_t reset_address = reset & ~1u;
    return sp >= CAN_OTA_AB_SRAM_START &&
           sp <= CAN_OTA_AB_SRAM_END &&
           (reset & 1u) != 0u &&
           reset_address >= slot_address &&
           reset_address < slot_address + image_size;
}

int ota_ab_package_parse(const uint8_t *data, size_t size,
                         ota_ab_package_t *package,
                         char *error, size_t error_size)
{
    uint32_t a_size;
    uint32_t b_size;
    uint16_t a_block_count;
    uint16_t b_block_count;
    uint8_t digest[CAN_OTA_AB_SHA256_SIZE];
    size_t expected_size;

    if (data == NULL || package == NULL) {
        return fail(error, error_size, "invalid parser argument");
    }
    if (size < CAN_OTA_AB_PACKAGE_HEADER_SIZE) {
        return fail(error, error_size, "file is smaller than OTA3 header");
    }
    if (get_le32(data + CAN_OTA_AB_PACKAGE_OFF_MAGIC) !=
        CAN_OTA_AB_PACKAGE_MAGIC) {
        return fail(error, error_size,
                    "invalid OTA3 magic (older packages and raw .bin are rejected)");
    }
    if (get_le16(data + CAN_OTA_AB_PACKAGE_OFF_HEADER_SIZE) !=
            CAN_OTA_AB_PACKAGE_HEADER_SIZE ||
        data[CAN_OTA_AB_PACKAGE_OFF_FORMAT] !=
            CAN_OTA_AB_PACKAGE_FORMAT_VERSION ||
        data[CAN_OTA_AB_PACKAGE_OFF_SIGNATURE_ALG] !=
            CAN_OTA_AB_SIGNATURE_ECDSA_P256 ||
        data[CAN_OTA_AB_PACKAGE_OFF_HASH_ALG] !=
            CAN_OTA_AB_HASH_SHA256) {
        return fail(error, error_size, "unsupported OTA3 algorithms or format");
    }
    if ((data[CAN_OTA_AB_PACKAGE_OFF_FLAGS] &
         (uint8_t)~CAN_OTA_AB_MANIFEST_KNOWN_FLAGS) != 0u ||
        get_le32(data + CAN_OTA_AB_PACKAGE_OFF_RESERVED) != 0u ||
        get_le16(data + CAN_OTA_AB_PACKAGE_OFF_A_RESERVED) != 0u ||
        get_le16(data + CAN_OTA_AB_PACKAGE_OFF_B_RESERVED) != 0u ||
        get_le32(data + CAN_OTA_AB_PACKAGE_OFF_BLOCK_SIZE) !=
            CAN_OTA_AB_BLOCK_SIZE) {
        return fail(error, error_size, "unknown flags or reserved field");
    }
    if (ota_ab_package_crc32(data, CAN_OTA_AB_PACKAGE_CRC_BYTES) !=
        get_le32(data + CAN_OTA_AB_PACKAGE_OFF_HEADER_CRC32)) {
        return fail(error, error_size, "OTA3 header CRC32 mismatch");
    }

    a_size = get_le32(data + CAN_OTA_AB_PACKAGE_OFF_A_SIZE);
    b_size = get_le32(data + CAN_OTA_AB_PACKAGE_OFF_B_SIZE);
    if (a_size < 8u || a_size > CAN_OTA_AB_SLOT_SIZE ||
        b_size < 8u || b_size > CAN_OTA_AB_SLOT_SIZE) {
        return fail(error, error_size, "invalid A/B image size");
    }
    a_block_count =
        get_le16(data + CAN_OTA_AB_PACKAGE_OFF_A_BLOCK_COUNT);
    b_block_count =
        get_le16(data + CAN_OTA_AB_PACKAGE_OFF_B_BLOCK_COUNT);
    if (a_block_count !=
            (a_size + CAN_OTA_AB_BLOCK_SIZE - 1u) /
                CAN_OTA_AB_BLOCK_SIZE ||
        b_block_count !=
            (b_size + CAN_OTA_AB_BLOCK_SIZE - 1u) /
                CAN_OTA_AB_BLOCK_SIZE) {
        return fail(error, error_size, "invalid A/B block count");
    }
    expected_size = CAN_OTA_AB_PACKAGE_HEADER_SIZE +
                    ((size_t)a_block_count + (size_t)b_block_count) *
                        CAN_OTA_AB_SHA256_SIZE +
                    (size_t)a_size + (size_t)b_size;
    if (expected_size != size) {
        return fail(error, error_size, "OTA3 file length does not match header");
    }

    memset(package, 0, sizeof(*package));
    package->header = data;
    package->flags = data[CAN_OTA_AB_PACKAGE_OFF_FLAGS];
    package->hardware_id =
        get_le16(data + CAN_OTA_AB_PACKAGE_OFF_HARDWARE_ID);
    package->firmware_version =
        get_le32(data + CAN_OTA_AB_PACKAGE_OFF_VERSION);
    package->key_id = get_le32(data + CAN_OTA_AB_PACKAGE_OFF_KEY_ID);
    package->block_counts[CAN_OTA_AB_SLOT_A] = a_block_count;
    package->block_counts[CAN_OTA_AB_SLOT_B] = b_block_count;
    package->signature = data + CAN_OTA_AB_PACKAGE_OFF_SIGNATURE;
    package->block_hash_tables[CAN_OTA_AB_SLOT_A] =
        data + CAN_OTA_AB_PACKAGE_HEADER_SIZE;
    package->block_hash_tables[CAN_OTA_AB_SLOT_B] =
        package->block_hash_tables[CAN_OTA_AB_SLOT_A] +
        (size_t)package->block_counts[CAN_OTA_AB_SLOT_A] *
            CAN_OTA_AB_SHA256_SIZE;
    package->images[CAN_OTA_AB_SLOT_A] =
        package->block_hash_tables[CAN_OTA_AB_SLOT_B] +
        (size_t)package->block_counts[CAN_OTA_AB_SLOT_B] *
            CAN_OTA_AB_SHA256_SIZE;
    package->images[CAN_OTA_AB_SLOT_B] =
        package->images[CAN_OTA_AB_SLOT_A] + a_size;
    package->image_sizes[CAN_OTA_AB_SLOT_A] = a_size;
    package->image_sizes[CAN_OTA_AB_SLOT_B] = b_size;
    package->image_crc32[CAN_OTA_AB_SLOT_A] =
        get_le32(data + CAN_OTA_AB_PACKAGE_OFF_A_CRC32);
    package->image_crc32[CAN_OTA_AB_SLOT_B] =
        get_le32(data + CAN_OTA_AB_PACKAGE_OFF_B_CRC32);
    memcpy(package->image_sha256[CAN_OTA_AB_SLOT_A],
           data + CAN_OTA_AB_PACKAGE_OFF_A_SHA256, sizeof(digest));
    memcpy(package->image_sha256[CAN_OTA_AB_SLOT_B],
           data + CAN_OTA_AB_PACKAGE_OFF_B_SHA256, sizeof(digest));

    ota_sha256(package->block_hash_tables[CAN_OTA_AB_SLOT_A],
               (size_t)package->block_counts[CAN_OTA_AB_SLOT_A] *
                   CAN_OTA_AB_SHA256_SIZE,
               digest);
    if (memcmp(digest,
               data + CAN_OTA_AB_PACKAGE_OFF_A_TABLE_SHA,
               sizeof(digest)) != 0) {
        return fail(error, error_size, "Slot A block hash table mismatch");
    }
    ota_sha256(package->block_hash_tables[CAN_OTA_AB_SLOT_B],
               (size_t)package->block_counts[CAN_OTA_AB_SLOT_B] *
                   CAN_OTA_AB_SHA256_SIZE,
               digest);
    if (memcmp(digest,
               data + CAN_OTA_AB_PACKAGE_OFF_B_TABLE_SHA,
               sizeof(digest)) != 0) {
        return fail(error, error_size, "Slot B block hash table mismatch");
    }

    if (package->hardware_id == 0u ||
        package->firmware_version == 0u ||
        package->key_id == 0u) {
        return fail(error, error_size, "missing hardware, version or key ID");
    }
    if (ota_ab_package_crc32(
            package->images[CAN_OTA_AB_SLOT_A], a_size) !=
            package->image_crc32[CAN_OTA_AB_SLOT_A] ||
        ota_ab_package_crc32(
            package->images[CAN_OTA_AB_SLOT_B], b_size) !=
            package->image_crc32[CAN_OTA_AB_SLOT_B]) {
        return fail(error, error_size, "A/B image CRC32 mismatch");
    }
    ota_sha256(package->images[CAN_OTA_AB_SLOT_A], a_size, digest);
    if (memcmp(digest, package->image_sha256[CAN_OTA_AB_SLOT_A],
               sizeof(digest)) != 0) {
        return fail(error, error_size, "Slot A SHA-256 mismatch");
    }
    ota_sha256(package->images[CAN_OTA_AB_SLOT_B], b_size, digest);
    if (memcmp(digest, package->image_sha256[CAN_OTA_AB_SLOT_B],
               sizeof(digest)) != 0) {
        return fail(error, error_size, "Slot B SHA-256 mismatch");
    }
    {
        uint8_t slot;
        for (slot = CAN_OTA_AB_SLOT_A;
             slot <= CAN_OTA_AB_SLOT_B; slot++) {
            uint16_t block;
            for (block = 0u; block < package->block_counts[slot]; block++) {
                size_t offset = (size_t)block * CAN_OTA_AB_BLOCK_SIZE;
                size_t remaining = package->image_sizes[slot] - offset;
                size_t length = remaining < CAN_OTA_AB_BLOCK_SIZE ?
                                remaining : CAN_OTA_AB_BLOCK_SIZE;
                ota_sha256(package->images[slot] + offset, length, digest);
                if (memcmp(digest,
                           package->block_hash_tables[slot] +
                               (size_t)block * CAN_OTA_AB_SHA256_SIZE,
                           sizeof(digest)) != 0) {
                    return fail(error, error_size,
                                "image block SHA-256 mismatch");
                }
            }
        }
    }
    if (!validate_vector(package->images[CAN_OTA_AB_SLOT_A],
                         a_size, CAN_OTA_AB_SLOT_A_ADDRESS) ||
        !validate_vector(package->images[CAN_OTA_AB_SLOT_B],
                         b_size, CAN_OTA_AB_SLOT_B_ADDRESS)) {
        return fail(error, error_size,
                    "A/B vector table does not match its linked slot");
    }
    return 0;
}
