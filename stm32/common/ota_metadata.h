#ifndef OTA_METADATA_H
#define OTA_METADATA_H

#include <stdint.h>

#include "../../common/ota_contract.h"

/* STM32F103ZET6: 512 KiB Flash, 2 KiB page size. */
#define OTA_FLASH_BASE_ADDR        0x08000000u
#define OTA_FLASH_END_ADDR         0x08080000u
#define OTA_FLASH_PAGE_SIZE        0x00000800u
#define OTA_APP_ADDR               CAN_OTA_APP_ADDRESS
#define OTA_METADATA_ADDR          (OTA_FLASH_END_ADDR - OTA_FLASH_PAGE_SIZE)
#define OTA_APP_MAX_SIZE           CAN_OTA_APP_MAX_SIZE

#define OTA_METADATA_MAGIC         0x4D41544Fu /* ASCII "OTAM" */
#define OTA_METADATA_FORMAT        1u

/* Logical states reported by ota_metadata_current_state(). */
#define OTA_IMAGE_STATE_PENDING    0xFFFEu
#define OTA_IMAGE_STATE_TRIAL      0xFFFCu
#define OTA_IMAGE_STATE_CONFIRMED  0xFFF8u

/*
 * STM32F1 rejects programming a non-erased half-word (except 0x0000).
 * Trial and confirmation therefore use separate, initially erased slots
 * immediately following ota_boot_metadata_t. Each slot is programmed once.
 */
#define OTA_METADATA_MARKER_ERASED      0xFFFFu
#define OTA_METADATA_TRIAL_MARKER       0xA55Au
#define OTA_METADATA_CONFIRMED_MARKER   0x5AA5u
#define OTA_METADATA_TRIAL_MARKER_ADDR \
    (OTA_METADATA_ADDR + (uint32_t)sizeof(ota_boot_metadata_t))
#define OTA_METADATA_CONFIRMED_MARKER_ADDR \
    (OTA_METADATA_TRIAL_MARKER_ADDR + 2u)

typedef struct {
    uint32_t magic;
    uint16_t format_version;
    uint16_t hardware_id;
    uint32_t firmware_version;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t immutable_crc32;
    uint16_t state;
    uint16_t reserved;
} ota_boot_metadata_t;

typedef enum {
    OTA_METADATA_RESULT_OK = 0,
    OTA_METADATA_RESULT_EMPTY = 1,
    OTA_METADATA_RESULT_INVALID = 2,
    OTA_METADATA_RESULT_STATE = 3,
    OTA_METADATA_RESULT_FLASH = 4
} ota_metadata_result_t;

const ota_boot_metadata_t *ota_metadata_get(void);
uint8_t ota_metadata_is_blank(void);
uint8_t ota_metadata_is_valid(const ota_boot_metadata_t *metadata);
uint8_t ota_metadata_is_confirmed(const ota_boot_metadata_t *metadata);
uint16_t ota_metadata_current_state(const ota_boot_metadata_t *metadata);
uint32_t ota_metadata_crc32(const ota_boot_metadata_t *metadata);

ota_metadata_result_t ota_metadata_write_pending(uint16_t hardware_id,
                                                 uint32_t firmware_version,
                                                 uint32_t image_size,
                                                 uint32_t image_crc32);
ota_metadata_result_t ota_metadata_mark_trial(void);
ota_metadata_result_t ota_metadata_confirm_running_image(void);

#endif
