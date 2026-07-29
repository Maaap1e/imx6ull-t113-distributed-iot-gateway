#ifndef OTA_CONTRACT_H
#define OTA_CONTRACT_H

#include <stdint.h>

/* CAN protocol shared by the Linux host and STM32 bootloader. */
#define CAN_APP_ID_HEARTBEAT              0x101u
#define CAN_APP_ID_CONTROL                0x201u
#define CAN_APP_CMD_ENTER_BOOT            0xA5u

#define CAN_OTA_ID_ENTER                  0x300u
#define CAN_OTA_ID_INFO                   0x301u
#define CAN_OTA_ID_DATA                   0x302u
#define CAN_OTA_ID_MANIFEST               0x303u
#define CAN_OTA_ID_STATUS                 0x380u

#define CAN_OTA_CMD_ENTER                 0xA5u
#define CAN_OTA_PROTOCOL_VERSION          1u
#define CAN_OTA_HARDWARE_ID_STM32F103     0xF103u

#define CAN_OTA_MANIFEST_ALLOW_DOWNGRADE  0x01u
#define CAN_OTA_MANIFEST_KNOWN_FLAGS      CAN_OTA_MANIFEST_ALLOW_DOWNGRADE
#define CAN_OTA_APP_ADDRESS               0x08010000u
#define CAN_OTA_APP_MAX_SIZE              0x0006F800u /* 446 KiB */
#define CAN_OTA_SRAM_START                 0x20000000u
#define CAN_OTA_SRAM_END                   0x20010000u

/*
 * Versions compare as unsigned integers in semantic order:
 * major.minor.patch.build. Each component is limited to 0..255.
 */
#define CAN_OTA_VERSION(major, minor, patch, build) \
    ((((uint32_t)(major) & 0xFFu) << 24) | \
     (((uint32_t)(minor) & 0xFFu) << 16) | \
     (((uint32_t)(patch) & 0xFFu) << 8) | \
     ((uint32_t)(build) & 0xFFu))

#define CAN_OTA_VERSION_MAJOR(version) ((uint8_t)((version) >> 24))
#define CAN_OTA_VERSION_MINOR(version) ((uint8_t)((version) >> 16))
#define CAN_OTA_VERSION_PATCH(version) ((uint8_t)((version) >> 8))
#define CAN_OTA_VERSION_BUILD(version) ((uint8_t)(version))

/* Portable .ota package header. All integer fields are little endian. */
#define CAN_OTA_PACKAGE_MAGIC             0x41544F43u /* ASCII "COTA" */
#define CAN_OTA_PACKAGE_HEADER_SIZE       32u
#define CAN_OTA_PACKAGE_FORMAT_VERSION    1u

#define CAN_OTA_PACKAGE_OFF_MAGIC         0u
#define CAN_OTA_PACKAGE_OFF_HEADER_SIZE   4u
#define CAN_OTA_PACKAGE_OFF_FORMAT        6u
#define CAN_OTA_PACKAGE_OFF_FLAGS         7u
#define CAN_OTA_PACKAGE_OFF_HARDWARE_ID   8u
#define CAN_OTA_PACKAGE_OFF_RESERVED0     10u
#define CAN_OTA_PACKAGE_OFF_VERSION       12u
#define CAN_OTA_PACKAGE_OFF_IMAGE_SIZE    16u
#define CAN_OTA_PACKAGE_OFF_IMAGE_CRC32   20u
#define CAN_OTA_PACKAGE_OFF_RESERVED1     24u
#define CAN_OTA_PACKAGE_OFF_HEADER_CRC32  28u
#define CAN_OTA_PACKAGE_CRC_BYTES         28u

#endif
