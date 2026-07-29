#include "ota_metadata.h"

#include <stddef.h>

#include "stm32f1xx_hal.h"

typedef char ota_metadata_size_must_be_28_bytes[
    sizeof(ota_boot_metadata_t) == 28u ? 1 : -1
];
typedef char ota_app_slot_must_end_at_metadata_page[
    (OTA_APP_ADDR + OTA_APP_MAX_SIZE) == OTA_METADATA_ADDR ? 1 : -1
];
typedef char ota_metadata_markers_must_fit_page[
    (OTA_METADATA_CONFIRMED_MARKER_ADDR + 2u) <= OTA_FLASH_END_ADDR ? 1 : -1
];

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    uint32_t bit;

    crc = ~crc;
    while (len-- > 0u) {
        crc ^= *data++;
        for (bit = 0; bit < 8u; bit++) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static uint32_t crc32_le16(uint32_t crc, uint16_t value)
{
    uint8_t bytes[2];

    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)(value >> 8);
    return crc32_update(crc, bytes, sizeof(bytes));
}

static uint32_t crc32_le32(uint32_t crc, uint32_t value)
{
    uint8_t bytes[4];

    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFu);
    bytes[2] = (uint8_t)((value >> 16) & 0xFFu);
    bytes[3] = (uint8_t)((value >> 24) & 0xFFu);
    return crc32_update(crc, bytes, sizeof(bytes));
}

const ota_boot_metadata_t *ota_metadata_get(void)
{
    return (const ota_boot_metadata_t *)OTA_METADATA_ADDR;
}

uint8_t ota_metadata_is_blank(void)
{
    return *(volatile const uint32_t *)OTA_METADATA_ADDR == 0xFFFFFFFFu;
}

uint32_t ota_metadata_crc32(const ota_boot_metadata_t *metadata)
{
    uint32_t crc = 0u;

    crc = crc32_le32(crc, metadata->magic);
    crc = crc32_le16(crc, metadata->format_version);
    crc = crc32_le16(crc, metadata->hardware_id);
    crc = crc32_le32(crc, metadata->firmware_version);
    crc = crc32_le32(crc, metadata->image_size);
    crc = crc32_le32(crc, metadata->image_crc32);
    return crc;
}

uint8_t ota_metadata_is_valid(const ota_boot_metadata_t *metadata)
{
    if (metadata->magic != OTA_METADATA_MAGIC ||
        metadata->format_version != OTA_METADATA_FORMAT ||
        metadata->hardware_id != CAN_OTA_HARDWARE_ID_STM32F103 ||
        metadata->firmware_version == 0u ||
        metadata->image_size < 8u ||
        metadata->image_size > OTA_APP_MAX_SIZE) {
        return 0u;
    }

    if (metadata->state != OTA_IMAGE_STATE_PENDING &&
        metadata->state != OTA_IMAGE_STATE_TRIAL &&
        metadata->state != OTA_IMAGE_STATE_CONFIRMED) {
        return 0u;
    }

    return metadata->immutable_crc32 == ota_metadata_crc32(metadata);
}

uint8_t ota_metadata_is_confirmed(const ota_boot_metadata_t *metadata)
{
    return ota_metadata_is_valid(metadata) &&
           ota_metadata_current_state(metadata) == OTA_IMAGE_STATE_CONFIRMED;
}

uint16_t ota_metadata_current_state(const ota_boot_metadata_t *metadata)
{
    uint16_t trial_marker;
    uint16_t confirmed_marker;

    /*
     * Accept metadata produced by the first v1.2 prototype so a device left
     * in recovery mode can still receive a replacement OTA package.
     */
    if (metadata->state == OTA_IMAGE_STATE_TRIAL ||
        metadata->state == OTA_IMAGE_STATE_CONFIRMED) {
        return metadata->state;
    }
    if (metadata->state != OTA_IMAGE_STATE_PENDING) {
        return 0u;
    }

    trial_marker =
        *(volatile const uint16_t *)OTA_METADATA_TRIAL_MARKER_ADDR;
    confirmed_marker =
        *(volatile const uint16_t *)OTA_METADATA_CONFIRMED_MARKER_ADDR;

    if (confirmed_marker == OTA_METADATA_CONFIRMED_MARKER) {
        return trial_marker == OTA_METADATA_TRIAL_MARKER
                   ? OTA_IMAGE_STATE_CONFIRMED
                   : 0u;
    }
    if (confirmed_marker != OTA_METADATA_MARKER_ERASED) {
        return 0u;
    }
    if (trial_marker == OTA_METADATA_TRIAL_MARKER) {
        return OTA_IMAGE_STATE_TRIAL;
    }
    if (trial_marker != OTA_METADATA_MARKER_ERASED) {
        return 0u;
    }
    return OTA_IMAGE_STATE_PENDING;
}

static ota_metadata_result_t erase_metadata_page(void)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0u;
    HAL_StatusTypeDef status;

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks = FLASH_BANK_1;
    erase.PageAddress = OTA_METADATA_ADDR;
    erase.NbPages = 1u;

    HAL_FLASH_Unlock();
    status = HAL_FLASHEx_Erase(&erase, &page_error);
    HAL_FLASH_Lock();

    if (status != HAL_OK || page_error != 0xFFFFFFFFu) {
        return OTA_METADATA_RESULT_FLASH;
    }
    return OTA_METADATA_RESULT_OK;
}

static ota_metadata_result_t program_halfwords(uint32_t address,
                                               const uint16_t *data,
                                               uint16_t count)
{
    uint16_t i;
    HAL_StatusTypeDef status = HAL_OK;

    HAL_FLASH_Unlock();
    for (i = 0u; i < count; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                                   address + ((uint32_t)i * 2u),
                                   data[i]);
        if (status != HAL_OK) {
            break;
        }
    }
    HAL_FLASH_Lock();

    return status == HAL_OK ? OTA_METADATA_RESULT_OK
                            : OTA_METADATA_RESULT_FLASH;
}

ota_metadata_result_t ota_metadata_write_pending(uint16_t hardware_id,
                                                 uint32_t firmware_version,
                                                 uint32_t image_size,
                                                 uint32_t image_crc32)
{
    ota_boot_metadata_t metadata;
    ota_metadata_result_t result;

    if (hardware_id != CAN_OTA_HARDWARE_ID_STM32F103 ||
        firmware_version == 0u ||
        image_size < 8u ||
        image_size > OTA_APP_MAX_SIZE) {
        return OTA_METADATA_RESULT_INVALID;
    }

    metadata.magic = OTA_METADATA_MAGIC;
    metadata.format_version = OTA_METADATA_FORMAT;
    metadata.hardware_id = hardware_id;
    metadata.firmware_version = firmware_version;
    metadata.image_size = image_size;
    metadata.image_crc32 = image_crc32;
    metadata.immutable_crc32 = ota_metadata_crc32(&metadata);
    metadata.state = OTA_IMAGE_STATE_PENDING;
    metadata.reserved = 0xFFFFu;

    result = erase_metadata_page();
    if (result != OTA_METADATA_RESULT_OK) {
        return result;
    }

    /*
     * Do not program the erased reserved field or the two state-marker slots.
     * The slots must remain 0xFFFF until their one-time transition writes.
     */
    result = program_halfwords(
        OTA_METADATA_ADDR,
        (const uint16_t *)&metadata,
        (uint16_t)(offsetof(ota_boot_metadata_t, reserved) / 2u));
    if (result != OTA_METADATA_RESULT_OK) {
        return result;
    }

    return ota_metadata_is_valid(ota_metadata_get()) &&
                   ota_metadata_current_state(ota_metadata_get()) ==
                       OTA_IMAGE_STATE_PENDING
               ? OTA_METADATA_RESULT_OK
               : OTA_METADATA_RESULT_FLASH;
}

static ota_metadata_result_t transition_state(uint16_t next_state)
{
    const ota_boot_metadata_t *metadata = ota_metadata_get();
    uint16_t current_state;
    uint16_t marker;
    uint32_t marker_address;
    ota_metadata_result_t result;

    if (ota_metadata_is_blank()) {
        return OTA_METADATA_RESULT_EMPTY;
    }
    if (!ota_metadata_is_valid(metadata)) {
        return OTA_METADATA_RESULT_INVALID;
    }

    current_state = ota_metadata_current_state(metadata);
    if (next_state == OTA_IMAGE_STATE_TRIAL) {
        if (current_state != OTA_IMAGE_STATE_PENDING) {
            return OTA_METADATA_RESULT_STATE;
        }
        marker = OTA_METADATA_TRIAL_MARKER;
        marker_address = OTA_METADATA_TRIAL_MARKER_ADDR;
    } else if (next_state == OTA_IMAGE_STATE_CONFIRMED) {
        if (current_state == OTA_IMAGE_STATE_CONFIRMED) {
            return OTA_METADATA_RESULT_OK;
        }
        if (current_state != OTA_IMAGE_STATE_TRIAL) {
            return OTA_METADATA_RESULT_STATE;
        }
        marker = OTA_METADATA_CONFIRMED_MARKER;
        marker_address = OTA_METADATA_CONFIRMED_MARKER_ADDR;
    } else {
        return OTA_METADATA_RESULT_STATE;
    }

    if (*(volatile const uint16_t *)marker_address !=
        OTA_METADATA_MARKER_ERASED) {
        return OTA_METADATA_RESULT_STATE;
    }

    result = program_halfwords(marker_address, &marker, 1u);
    if (result != OTA_METADATA_RESULT_OK) {
        return result;
    }

    metadata = ota_metadata_get();
    return ota_metadata_current_state(metadata) == next_state &&
                   ota_metadata_is_valid(metadata)
               ? OTA_METADATA_RESULT_OK
               : OTA_METADATA_RESULT_FLASH;
}

ota_metadata_result_t ota_metadata_mark_trial(void)
{
    return transition_state(OTA_IMAGE_STATE_TRIAL);
}

ota_metadata_result_t ota_metadata_confirm_running_image(void)
{
    return transition_state(OTA_IMAGE_STATE_CONFIRMED);
}
