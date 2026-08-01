#include "ota_ab_metadata.h"

#include <stddef.h>
#include <string.h>

#include "stm32f1xx_hal.h"

typedef char ota_ab_record_must_be_128_bytes[
    sizeof(ota_ab_metadata_t) == OTA_AB_METADATA_RECORD_SIZE ? 1 : -1
];
typedef char ota_ab_slot_a_must_end_at_slot_b[
    CAN_OTA_AB_SLOT_A_ADDRESS + CAN_OTA_AB_SLOT_SIZE ==
    CAN_OTA_AB_SLOT_B_ADDRESS ? 1 : -1
];
typedef char ota_ab_slot_b_must_end_at_metadata[
    CAN_OTA_AB_SLOT_B_ADDRESS + CAN_OTA_AB_SLOT_SIZE ==
    CAN_OTA_AB_METADATA0_ADDRESS ? 1 : -1
];

#define RECORDS_PER_PAGE \
    (CAN_OTA_AB_FLASH_PAGE_SIZE / OTA_AB_METADATA_RECORD_SIZE)
#define TOTAL_RECORDS (RECORDS_PER_PAGE * 2u)

/* v1 single-slot metadata retained only for one-time migration. */
#define LEGACY_METADATA_MAGIC      0x4D41544Fu
#define LEGACY_METADATA_FORMAT     1u
#define LEGACY_PENDING             0xFFFEu
#define LEGACY_CONFIRMED           0xFFF8u
#define LEGACY_TRIAL_MARKER        0xA55Au
#define LEGACY_CONFIRMED_MARKER    0x5AA5u

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
} legacy_metadata_t;

uint32_t ota_ab_crc32(const void *data, uint32_t len)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;

    while (len-- > 0u) {
        uint32_t bit;

        crc ^= *bytes++;
        for (bit = 0u; bit < 8u; bit++) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

uint32_t ota_ab_slot_address(uint8_t slot)
{
    if (slot == CAN_OTA_AB_SLOT_A) {
        return CAN_OTA_AB_SLOT_A_ADDRESS;
    }
    if (slot == CAN_OTA_AB_SLOT_B) {
        return CAN_OTA_AB_SLOT_B_ADDRESS;
    }
    return 0u;
}

uint8_t ota_ab_other_slot(uint8_t slot)
{
    return slot == CAN_OTA_AB_SLOT_A ?
           CAN_OTA_AB_SLOT_B : CAN_OTA_AB_SLOT_A;
}

uint8_t ota_ab_slot_id_is_valid(uint8_t slot)
{
    return slot == CAN_OTA_AB_SLOT_A || slot == CAN_OTA_AB_SLOT_B;
}

static uint32_t record_address(uint32_t index)
{
    if (index < RECORDS_PER_PAGE) {
        return CAN_OTA_AB_METADATA0_ADDRESS +
               index * OTA_AB_METADATA_RECORD_SIZE;
    }
    return CAN_OTA_AB_METADATA1_ADDRESS +
           (index - RECORDS_PER_PAGE) * OTA_AB_METADATA_RECORD_SIZE;
}

static uint8_t sequence_is_newer(uint32_t candidate, uint32_t current)
{
    return (int32_t)(candidate - current) > 0;
}

uint8_t ota_ab_metadata_record_is_valid(const ota_ab_metadata_t *record)
{
    uint32_t expected_crc;

    if (record->magic != OTA_AB_METADATA_MAGIC ||
        record->format_version != OTA_AB_METADATA_FORMAT ||
        record->record_size != OTA_AB_METADATA_RECORD_SIZE ||
        !ota_ab_slot_id_is_valid(record->active_slot) ||
        (record->confirmed_slot != CAN_OTA_AB_SLOT_NONE &&
         !ota_ab_slot_id_is_valid(record->confirmed_slot)) ||
        (record->candidate_slot != CAN_OTA_AB_SLOT_NONE &&
         !ota_ab_slot_id_is_valid(record->candidate_slot)) ||
        record->trial_booted > 1u) {
        return 0u;
    }

    if (record->candidate_slot == CAN_OTA_AB_SLOT_NONE &&
        record->trial_booted != 0u) {
        return 0u;
    }
    if (record->candidate_slot != CAN_OTA_AB_SLOT_NONE &&
        record->candidate_slot != record->active_slot) {
        return 0u;
    }

    expected_crc = ota_ab_crc32(
        record, (uint32_t)offsetof(ota_ab_metadata_t, record_crc32));
    return expected_crc == record->record_crc32;
}

ota_ab_meta_result_t ota_ab_metadata_load(ota_ab_metadata_t *record)
{
    const ota_ab_metadata_t *latest = NULL;
    uint32_t index;

    if (record == NULL) {
        return OTA_AB_META_INVALID;
    }

    for (index = 0u; index < TOTAL_RECORDS; index++) {
        const ota_ab_metadata_t *candidate =
            (const ota_ab_metadata_t *)record_address(index);

        if (!ota_ab_metadata_record_is_valid(candidate)) {
            continue;
        }
        if (latest == NULL ||
            sequence_is_newer(candidate->sequence, latest->sequence)) {
            latest = candidate;
        }
    }

    if (latest == NULL) {
        memset(record, 0, sizeof(*record));
        return OTA_AB_META_EMPTY;
    }
    memcpy(record, latest, sizeof(*record));
    return OTA_AB_META_OK;
}

static uint8_t record_is_erased(uint32_t address)
{
    uint32_t offset;

    for (offset = 0u; offset < OTA_AB_METADATA_RECORD_SIZE; offset += 4u) {
        if (*(volatile const uint32_t *)(address + offset) != 0xFFFFFFFFu) {
            return 0u;
        }
    }
    return 1u;
}

static ota_ab_meta_result_t erase_page(uint32_t address)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0u;

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks = FLASH_BANK_1;
    erase.PageAddress = address;
    erase.NbPages = 1u;

    HAL_FLASH_Unlock();
    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK ||
        page_error != 0xFFFFFFFFu) {
        HAL_FLASH_Lock();
        return OTA_AB_META_FLASH;
    }
    HAL_FLASH_Lock();
    return OTA_AB_META_OK;
}

static ota_ab_meta_result_t program_record(
    uint32_t address, const ota_ab_metadata_t *record)
{
    const uint16_t *halfwords = (const uint16_t *)record;
    uint32_t halfword_count = sizeof(*record) / 2u;
    uint32_t index;

    HAL_FLASH_Unlock();

    /* Write everything except magic first. Magic is the commit marker. */
    for (index = 2u; index < halfword_count; index++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                              address + index * 2u,
                              halfwords[index]) != HAL_OK) {
            HAL_FLASH_Lock();
            return OTA_AB_META_FLASH;
        }
    }
    for (index = 0u; index < 2u; index++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                              address + index * 2u,
                              halfwords[index]) != HAL_OK) {
            HAL_FLASH_Lock();
            return OTA_AB_META_FLASH;
        }
    }
    HAL_FLASH_Lock();

    return ota_ab_metadata_record_is_valid(
               (const ota_ab_metadata_t *)address) ?
           OTA_AB_META_OK : OTA_AB_META_FLASH;
}

ota_ab_meta_result_t ota_ab_metadata_append(const ota_ab_metadata_t *record)
{
    ota_ab_metadata_t prepared;
    ota_ab_metadata_t latest;
    ota_ab_meta_result_t load_result;
    uint32_t latest_address = 0u;
    uint32_t target_address = 0u;
    uint32_t index;

    if (record == NULL) {
        return OTA_AB_META_INVALID;
    }

    load_result = ota_ab_metadata_load(&latest);
    if (load_result == OTA_AB_META_OK) {
        for (index = 0u; index < TOTAL_RECORDS; index++) {
            const ota_ab_metadata_t *candidate =
                (const ota_ab_metadata_t *)record_address(index);
            if (ota_ab_metadata_record_is_valid(candidate) &&
                candidate->sequence == latest.sequence) {
                latest_address = record_address(index);
                break;
            }
        }
    } else if (load_result != OTA_AB_META_EMPTY) {
        return load_result;
    }

    for (index = 0u; index < TOTAL_RECORDS; index++) {
        uint32_t address = record_address(index);
        if (record_is_erased(address)) {
            target_address = address;
            break;
        }
    }

    if (target_address == 0u) {
        uint32_t erase_address =
            latest_address >= CAN_OTA_AB_METADATA1_ADDRESS ?
            CAN_OTA_AB_METADATA0_ADDRESS :
            CAN_OTA_AB_METADATA1_ADDRESS;
        ota_ab_meta_result_t erase_result = erase_page(erase_address);
        if (erase_result != OTA_AB_META_OK) {
            return erase_result;
        }
        target_address = erase_address;
    }

    memcpy(&prepared, record, sizeof(prepared));
    prepared.magic = OTA_AB_METADATA_MAGIC;
    prepared.format_version = OTA_AB_METADATA_FORMAT;
    prepared.record_size = OTA_AB_METADATA_RECORD_SIZE;
    prepared.sequence = load_result == OTA_AB_META_OK ?
                        latest.sequence + 1u : 1u;
    memset(prepared.reserved, 0, sizeof(prepared.reserved));
    prepared.record_crc32 = ota_ab_crc32(
        &prepared, (uint32_t)offsetof(ota_ab_metadata_t, record_crc32));

    return program_record(target_address, &prepared);
}

static uint32_t legacy_crc(const legacy_metadata_t *legacy)
{
    return ota_ab_crc32(legacy, 20u);
}

ota_ab_meta_result_t ota_ab_metadata_import_legacy(void)
{
    const legacy_metadata_t *legacy =
        (const legacy_metadata_t *)CAN_OTA_AB_LEGACY_METADATA_ADDRESS;
    const volatile uint16_t *trial_marker =
        (const volatile uint16_t *)(CAN_OTA_AB_LEGACY_METADATA_ADDRESS +
                                    sizeof(legacy_metadata_t));
    const volatile uint16_t *confirmed_marker =
        trial_marker + 1;
    ota_ab_metadata_t record;
    uint8_t confirmed;

    if (legacy->magic != LEGACY_METADATA_MAGIC ||
        legacy->format_version != LEGACY_METADATA_FORMAT ||
        legacy->hardware_id != CAN_OTA_HARDWARE_ID_STM32F103 ||
        legacy->firmware_version == 0u ||
        legacy->image_size < 8u ||
        legacy->image_size > CAN_OTA_AB_SLOT_SIZE ||
        legacy->immutable_crc32 != legacy_crc(legacy)) {
        return OTA_AB_META_EMPTY;
    }

    confirmed = legacy->state == LEGACY_CONFIRMED ||
                (legacy->state == LEGACY_PENDING &&
                 *trial_marker == LEGACY_TRIAL_MARKER &&
                 *confirmed_marker == LEGACY_CONFIRMED_MARKER);
    if (!confirmed) {
        return OTA_AB_META_STATE;
    }

    memset(&record, 0, sizeof(record));
    record.active_slot = CAN_OTA_AB_SLOT_A;
    record.confirmed_slot = CAN_OTA_AB_SLOT_A;
    record.candidate_slot = CAN_OTA_AB_SLOT_NONE;
    record.slots[CAN_OTA_AB_SLOT_A].firmware_version =
        legacy->firmware_version;
    record.slots[CAN_OTA_AB_SLOT_A].image_size = legacy->image_size;
    record.slots[CAN_OTA_AB_SLOT_A].image_crc32 = legacy->image_crc32;
    record.slots[CAN_OTA_AB_SLOT_A].flags =
        OTA_AB_SLOT_FLAG_VALID | OTA_AB_SLOT_FLAG_LEGACY;
    return ota_ab_metadata_append(&record);
}

uint8_t ota_ab_metadata_choose_update_slot(const ota_ab_metadata_t *record)
{
    if (record != NULL &&
        ota_ab_metadata_record_is_valid(record) &&
        ota_ab_slot_id_is_valid(record->confirmed_slot)) {
        return ota_ab_other_slot(record->confirmed_slot);
    }
    return CAN_OTA_AB_SLOT_B;
}

ota_ab_meta_result_t ota_ab_metadata_set_candidate(
    uint8_t slot,
    uint32_t firmware_version,
    uint32_t image_size,
    uint32_t image_crc32,
    const uint8_t image_sha256[CAN_OTA_AB_SHA256_SIZE],
    uint32_t key_id)
{
    ota_ab_metadata_t record;
    ota_ab_meta_result_t result = ota_ab_metadata_load(&record);

    if (!ota_ab_slot_id_is_valid(slot) ||
        firmware_version == 0u ||
        image_size < 8u ||
        image_size > CAN_OTA_AB_SLOT_SIZE ||
        image_sha256 == NULL) {
        return OTA_AB_META_INVALID;
    }
    if (result == OTA_AB_META_EMPTY) {
        memset(&record, 0, sizeof(record));
        record.active_slot = slot;
        record.confirmed_slot = CAN_OTA_AB_SLOT_NONE;
        record.candidate_slot = CAN_OTA_AB_SLOT_NONE;
    } else if (result != OTA_AB_META_OK) {
        return result;
    }

    record.active_slot = slot;
    record.candidate_slot = slot;
    record.trial_booted = 0u;
    record.slots[slot].firmware_version = firmware_version;
    record.slots[slot].image_size = image_size;
    record.slots[slot].image_crc32 = image_crc32;
    record.slots[slot].flags =
        OTA_AB_SLOT_FLAG_VALID | OTA_AB_SLOT_FLAG_SIGNED;
    memcpy(record.slots[slot].image_sha256, image_sha256,
           CAN_OTA_AB_SHA256_SIZE);
    record.key_id = key_id;
    return ota_ab_metadata_append(&record);
}

ota_ab_meta_result_t ota_ab_metadata_mark_trial(uint8_t slot)
{
    ota_ab_metadata_t record;
    ota_ab_meta_result_t result = ota_ab_metadata_load(&record);

    if (result != OTA_AB_META_OK) {
        return result;
    }
    if (record.candidate_slot != slot ||
        record.active_slot != slot ||
        record.trial_booted != 0u) {
        return OTA_AB_META_STATE;
    }
    record.trial_booted = 1u;
    return ota_ab_metadata_append(&record);
}

ota_ab_meta_result_t ota_ab_metadata_confirm(uint8_t slot,
                                             uint32_t firmware_version)
{
    ota_ab_metadata_t record;
    ota_ab_meta_result_t result = ota_ab_metadata_load(&record);

    if (result != OTA_AB_META_OK) {
        return result;
    }
    /* Confirmation is idempotent across every later cold boot. */
    if (record.candidate_slot == CAN_OTA_AB_SLOT_NONE &&
        record.active_slot == slot &&
        record.confirmed_slot == slot &&
        record.slots[slot].firmware_version == firmware_version) {
        return OTA_AB_META_OK;
    }
    if (record.candidate_slot != slot ||
        record.active_slot != slot ||
        record.trial_booted != 1u ||
        record.slots[slot].firmware_version != firmware_version) {
        return OTA_AB_META_STATE;
    }

    record.confirmed_slot = slot;
    record.candidate_slot = CAN_OTA_AB_SLOT_NONE;
    record.trial_booted = 0u;
    return ota_ab_metadata_append(&record);
}

ota_ab_meta_result_t ota_ab_metadata_rollback(void)
{
    ota_ab_metadata_t record;
    ota_ab_meta_result_t result = ota_ab_metadata_load(&record);

    if (result != OTA_AB_META_OK) {
        return result;
    }
    if (!ota_ab_slot_id_is_valid(record.confirmed_slot)) {
        return OTA_AB_META_STATE;
    }

    record.active_slot = record.confirmed_slot;
    record.candidate_slot = CAN_OTA_AB_SLOT_NONE;
    record.trial_booted = 0u;
    return ota_ab_metadata_append(&record);
}
