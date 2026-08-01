#ifndef OTA_CONTRACT_AB_H
#define OTA_CONTRACT_AB_H

#include <stdint.h>

/*
 * Secure A/B CAN OTA protocol, package format v3.
 *
 * The application is linked twice because Cortex-M vector tables and code
 * contain absolute addresses. A package therefore carries one image linked
 * for Slot A and one image linked for Slot B. The bootloader selects and
 * writes only the inactive slot.
 */
#define CAN_APP_ID_HEARTBEAT                 0x101u
#define CAN_APP_ID_CONTROL                   0x201u
#define CAN_APP_CMD_ENTER_BOOT               0xA5u

#define CAN_OTA_AB_ID_ENTER                  0x300u
#define CAN_OTA_AB_ID_DATA                   0x302u
#define CAN_OTA_AB_ID_AUTH                   0x304u
#define CAN_OTA_AB_ID_BLOCK_HASH             0x305u
#define CAN_OTA_AB_ID_STATUS                 0x380u

#define CAN_OTA_AB_CMD_ENTER                 0xA5u
#define CAN_OTA_AB_PROTOCOL_VERSION          3u
#define CAN_OTA_HARDWARE_ID_STM32F103        0xF103u

#define CAN_OTA_AB_MANIFEST_ALLOW_DOWNGRADE  0x01u
#define CAN_OTA_AB_MANIFEST_KNOWN_FLAGS      \
    CAN_OTA_AB_MANIFEST_ALLOW_DOWNGRADE

#define CAN_OTA_AB_SLOT_A                    0u
#define CAN_OTA_AB_SLOT_B                    1u
#define CAN_OTA_AB_SLOT_NONE                 0xFFu

/* STM32F103ZET6: 512 KiB Flash, 2 KiB erase pages. */
#define CAN_OTA_AB_FLASH_BASE                0x08000000u
#define CAN_OTA_AB_FLASH_END                 0x08080000u
#define CAN_OTA_AB_FLASH_PAGE_SIZE           0x00000800u
#define CAN_OTA_AB_BOOT_SIZE                 0x00010000u
#define CAN_OTA_AB_SLOT_SIZE                 0x00037000u /* 220 KiB */
#define CAN_OTA_AB_SLOT_A_ADDRESS            0x08010000u
#define CAN_OTA_AB_SLOT_B_ADDRESS            0x08047000u
#define CAN_OTA_AB_METADATA0_ADDRESS         0x0807E000u
#define CAN_OTA_AB_METADATA1_ADDRESS         0x0807E800u
#define CAN_OTA_AB_RESUME0_ADDRESS           0x0807F000u
#define CAN_OTA_AB_RESUME1_ADDRESS           0x0807F800u
#define CAN_OTA_AB_LEGACY_METADATA_ADDRESS   0x0807F800u

#define CAN_OTA_AB_SRAM_START                0x20000000u
#define CAN_OTA_AB_SRAM_END                  0x20010000u

#define CAN_OTA_VERSION(major, minor, patch, build) \
    ((((uint32_t)(major) & 0xFFu) << 24) | \
     (((uint32_t)(minor) & 0xFFu) << 16) | \
     (((uint32_t)(patch) & 0xFFu) << 8) | \
     ((uint32_t)(build) & 0xFFu))

#define CAN_OTA_VERSION_MAJOR(version) ((uint8_t)((version) >> 24))
#define CAN_OTA_VERSION_MINOR(version) ((uint8_t)((version) >> 16))
#define CAN_OTA_VERSION_PATCH(version) ((uint8_t)((version) >> 8))
#define CAN_OTA_VERSION_BUILD(version) ((uint8_t)(version))

/*
 * Signed dual-image package header. All integers are little endian.
 * SHA-256 digests and raw ECDSA r||s are big-endian byte strings, matching
 * micro-ecc's public API.
 */
#define CAN_OTA_AB_PACKAGE_MAGIC             0x3341544Fu /* "OTA3" */
#define CAN_OTA_AB_PACKAGE_HEADER_SIZE       248u
#define CAN_OTA_AB_PACKAGE_FORMAT_VERSION    3u
#define CAN_OTA_AB_HASH_SHA256               1u
#define CAN_OTA_AB_SIGNATURE_ECDSA_P256      1u
#define CAN_OTA_AB_SHA256_SIZE               32u
#define CAN_OTA_AB_SIGNATURE_SIZE            64u
#define CAN_OTA_AB_BLOCK_SIZE                CAN_OTA_AB_FLASH_PAGE_SIZE
#define CAN_OTA_AB_MAX_BLOCKS                \
    (CAN_OTA_AB_SLOT_SIZE / CAN_OTA_AB_BLOCK_SIZE)
#define CAN_OTA_AB_BLOCK_HASH_TABLE_SIZE     \
    (CAN_OTA_AB_MAX_BLOCKS * CAN_OTA_AB_SHA256_SIZE)

#define CAN_OTA_AB_PACKAGE_OFF_MAGIC         0u
#define CAN_OTA_AB_PACKAGE_OFF_HEADER_SIZE   4u
#define CAN_OTA_AB_PACKAGE_OFF_FORMAT        6u
#define CAN_OTA_AB_PACKAGE_OFF_FLAGS         7u
#define CAN_OTA_AB_PACKAGE_OFF_HARDWARE_ID   8u
#define CAN_OTA_AB_PACKAGE_OFF_SIGNATURE_ALG 10u
#define CAN_OTA_AB_PACKAGE_OFF_HASH_ALG      11u
#define CAN_OTA_AB_PACKAGE_OFF_VERSION       12u
#define CAN_OTA_AB_PACKAGE_OFF_BLOCK_SIZE    16u
#define CAN_OTA_AB_PACKAGE_OFF_A_SIZE        20u
#define CAN_OTA_AB_PACKAGE_OFF_A_CRC32       24u
#define CAN_OTA_AB_PACKAGE_OFF_A_SHA256      28u
#define CAN_OTA_AB_PACKAGE_OFF_A_BLOCK_COUNT 60u
#define CAN_OTA_AB_PACKAGE_OFF_A_RESERVED    62u
#define CAN_OTA_AB_PACKAGE_OFF_A_TABLE_SHA   64u
#define CAN_OTA_AB_PACKAGE_OFF_B_SIZE        96u
#define CAN_OTA_AB_PACKAGE_OFF_B_CRC32       100u
#define CAN_OTA_AB_PACKAGE_OFF_B_SHA256      104u
#define CAN_OTA_AB_PACKAGE_OFF_B_BLOCK_COUNT 136u
#define CAN_OTA_AB_PACKAGE_OFF_B_RESERVED    138u
#define CAN_OTA_AB_PACKAGE_OFF_B_TABLE_SHA   140u
#define CAN_OTA_AB_PACKAGE_OFF_KEY_ID        172u
#define CAN_OTA_AB_PACKAGE_OFF_RESERVED      176u
#define CAN_OTA_AB_PACKAGE_OFF_SIGNATURE     180u
#define CAN_OTA_AB_PACKAGE_OFF_HEADER_CRC32  244u

/* The ECDSA signature authenticates bytes [0, AUTH_BYTES). */
#define CAN_OTA_AB_PACKAGE_AUTH_BYTES        180u
/* Header CRC32 detects transport damage and includes the signature. */
#define CAN_OTA_AB_PACKAGE_CRC_BYTES         244u

#define CAN_OTA_AB_STATUS_READY              0x01u
#define CAN_OTA_AB_STATUS_ERASING            0x02u
#define CAN_OTA_AB_STATUS_WRITING            0x03u
#define CAN_OTA_AB_STATUS_VERIFY             0x04u
#define CAN_OTA_AB_STATUS_DONE               0x05u
#define CAN_OTA_AB_STATUS_ROLLBACK            0x06u
#define CAN_OTA_AB_STATUS_ERROR              0xE0u

#define CAN_OTA_AB_ERR_NONE                  0x00u
#define CAN_OTA_AB_ERR_TIMEOUT               0x01u
#define CAN_OTA_AB_ERR_SIZE                  0x03u
#define CAN_OTA_AB_ERR_SEQ                   0x04u
#define CAN_OTA_AB_ERR_FLASH                 0x05u
#define CAN_OTA_AB_ERR_CRC                   0x06u
#define CAN_OTA_AB_ERR_APP                   0x07u
#define CAN_OTA_AB_ERR_MANIFEST              0x08u
#define CAN_OTA_AB_ERR_HARDWARE              0x09u
#define CAN_OTA_AB_ERR_ROLLBACK_POLICY       0x0Au
#define CAN_OTA_AB_ERR_METADATA              0x0Bu
#define CAN_OTA_AB_ERR_SHA256                0x0Cu
#define CAN_OTA_AB_ERR_SIGNATURE             0x0Du
#define CAN_OTA_AB_ERR_KEY_ID                0x0Eu
#define CAN_OTA_AB_ERR_SLOT                  0x0Fu
#define CAN_OTA_AB_ERR_BLOCK_HASH            0x10u
#define CAN_OTA_AB_ERR_RESUME                0x11u

#endif
