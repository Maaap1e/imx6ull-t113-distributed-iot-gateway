#include "ota_resume_journal.h"

#include <stddef.h>
#include <string.h>

#include "ota_ab_metadata.h"
#include "stm32f1xx_hal.h"

typedef char ota_resume_record_must_be_128_bytes[
    sizeof(ota_resume_record_t) == OTA_RESUME_RECORD_SIZE ? 1 : -1
];
typedef char ota_resume_bitmap_must_cover_slot[
    OTA_RESUME_BITMAP_BYTES * 8u >= CAN_OTA_AB_MAX_BLOCKS ? 1 : -1
];

#define RECORDS_PER_PAGE \
    (CAN_OTA_AB_FLASH_PAGE_SIZE / OTA_RESUME_RECORD_SIZE)
#define TOTAL_RECORDS (RECORDS_PER_PAGE * 2u)

static uint32_t record_address(uint32_t index)
{
    if (index < RECORDS_PER_PAGE) {
        return CAN_OTA_AB_RESUME0_ADDRESS +
               index * OTA_RESUME_RECORD_SIZE;
    }
    return CAN_OTA_AB_RESUME1_ADDRESS +
           (index - RECORDS_PER_PAGE) * OTA_RESUME_RECORD_SIZE;
}

static uint8_t sequence_is_newer(uint32_t candidate, uint32_t current)
{
    return (int32_t)(candidate - current) > 0;
}

static uint8_t bit_is_set(const ota_resume_record_t *record, uint16_t block)
{
    return (record->completed_bitmap[block / 8u] &
            (uint8_t)(1u << (block % 8u))) != 0u;
}

uint8_t ota_resume_record_is_valid(const ota_resume_record_t *record)
{
    uint16_t block;

    if (record->magic != OTA_RESUME_MAGIC ||
        record->format_version != OTA_RESUME_FORMAT ||
        record->record_size != OTA_RESUME_RECORD_SIZE ||
        (record->state != OTA_RESUME_STATE_RECEIVING &&
         record->state != OTA_RESUME_STATE_COMPLETE) ||
        !ota_ab_slot_id_is_valid(record->target_slot) ||
        record->block_size != CAN_OTA_AB_BLOCK_SIZE ||
        record->image_size < 8u ||
        record->image_size > CAN_OTA_AB_SLOT_SIZE ||
        record->block_count == 0u ||
        record->block_count > CAN_OTA_AB_MAX_BLOCKS ||
        record->block_count !=
            (record->image_size + CAN_OTA_AB_BLOCK_SIZE - 1u) /
                CAN_OTA_AB_BLOCK_SIZE ||
        ota_ab_crc32(record,
                     (uint32_t)offsetof(ota_resume_record_t, record_crc32)) !=
            record->record_crc32) {
        return 0u;
    }

    /* Only a contiguous prefix is accepted; this makes the resume offset exact. */
    for (block = ota_resume_first_missing(record);
         block < record->block_count; block++) {
        if (bit_is_set(record, block)) {
            return 0u;
        }
    }
    return record->state != OTA_RESUME_STATE_COMPLETE ||
           ota_resume_first_missing(record) == record->block_count;
}

ota_resume_result_t ota_resume_load(ota_resume_record_t *record)
{
    const ota_resume_record_t *latest = NULL;
    uint32_t index;

    if (record == NULL) {
        return OTA_RESUME_INVALID;
    }
    for (index = 0u; index < TOTAL_RECORDS; index++) {
        const ota_resume_record_t *candidate =
            (const ota_resume_record_t *)record_address(index);
        if (ota_resume_record_is_valid(candidate) &&
            (latest == NULL ||
             sequence_is_newer(candidate->sequence, latest->sequence))) {
            latest = candidate;
        }
    }
    if (latest == NULL) {
        memset(record, 0, sizeof(*record));
        return OTA_RESUME_EMPTY;
    }
    memcpy(record, latest, sizeof(*record));
    return OTA_RESUME_OK;
}

static uint8_t record_is_erased(uint32_t address)
{
    uint32_t offset;
    for (offset = 0u; offset < OTA_RESUME_RECORD_SIZE; offset += 4u) {
        if (*(volatile const uint32_t *)(address + offset) != 0xFFFFFFFFu) {
            return 0u;
        }
    }
    return 1u;
}

static ota_resume_result_t erase_page(uint32_t address)
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
        return OTA_RESUME_FLASH;
    }
    HAL_FLASH_Lock();
    return OTA_RESUME_OK;
}

static ota_resume_result_t program_record(
    uint32_t address, const ota_resume_record_t *record)
{
    const uint16_t *halfwords = (const uint16_t *)record;
    uint32_t count = sizeof(*record) / 2u;
    uint32_t index;

    HAL_FLASH_Unlock();
    /* Write everything except magic first; magic is the commit marker. */
    for (index = 2u; index < count; index++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                              address + index * 2u,
                              halfwords[index]) != HAL_OK) {
            HAL_FLASH_Lock();
            return OTA_RESUME_FLASH;
        }
    }
    for (index = 0u; index < 2u; index++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                              address + index * 2u,
                              halfwords[index]) != HAL_OK) {
            HAL_FLASH_Lock();
            return OTA_RESUME_FLASH;
        }
    }
    HAL_FLASH_Lock();
    return ota_resume_record_is_valid(
               (const ota_resume_record_t *)address) ?
           OTA_RESUME_OK : OTA_RESUME_FLASH;
}

ota_resume_result_t ota_resume_append(const ota_resume_record_t *record)
{
    ota_resume_record_t prepared;
    ota_resume_record_t latest;
    ota_resume_result_t load_result;
    uint32_t latest_address = 0u;
    uint32_t target_address = 0u;
    uint32_t index;

    if (record == NULL) {
        return OTA_RESUME_INVALID;
    }
    load_result = ota_resume_load(&latest);
    if (load_result == OTA_RESUME_OK) {
        for (index = 0u; index < TOTAL_RECORDS; index++) {
            const ota_resume_record_t *candidate =
                (const ota_resume_record_t *)record_address(index);
            if (ota_resume_record_is_valid(candidate) &&
                candidate->sequence == latest.sequence) {
                latest_address = record_address(index);
                break;
            }
        }
    } else if (load_result != OTA_RESUME_EMPTY) {
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
            latest_address >= CAN_OTA_AB_RESUME1_ADDRESS ?
            CAN_OTA_AB_RESUME0_ADDRESS : CAN_OTA_AB_RESUME1_ADDRESS;
        ota_resume_result_t result = erase_page(erase_address);
        if (result != OTA_RESUME_OK) {
            return result;
        }
        target_address = erase_address;
    }

    memcpy(&prepared, record, sizeof(prepared));
    prepared.magic = OTA_RESUME_MAGIC;
    prepared.format_version = OTA_RESUME_FORMAT;
    prepared.record_size = OTA_RESUME_RECORD_SIZE;
    prepared.sequence = load_result == OTA_RESUME_OK ?
                        latest.sequence + 1u : 1u;
    prepared.reserved0 = 0u;
    memset(prepared.reserved, 0, sizeof(prepared.reserved));
    prepared.record_crc32 = ota_ab_crc32(
        &prepared, (uint32_t)offsetof(ota_resume_record_t, record_crc32));
    return program_record(target_address, &prepared);
}

uint16_t ota_resume_first_missing(const ota_resume_record_t *record)
{
    uint16_t block = 0u;
    if (record == NULL) {
        return 0u;
    }
    while (block < record->block_count && bit_is_set(record, block)) {
        block++;
    }
    return block;
}

uint8_t ota_resume_matches(
    const ota_resume_record_t *record,
    uint8_t target_slot,
    uint32_t key_id,
    uint32_t firmware_version,
    uint32_t image_size,
    uint32_t image_crc32,
    uint16_t block_count,
    const uint8_t package_id[CAN_OTA_AB_SHA256_SIZE])
{
    return record != NULL && package_id != NULL &&
           ota_resume_record_is_valid(record) &&
           record->target_slot == target_slot &&
           record->key_id == key_id &&
           record->firmware_version == firmware_version &&
           record->image_size == image_size &&
           record->image_crc32 == image_crc32 &&
           record->block_count == block_count &&
           memcmp(record->package_id, package_id,
                  CAN_OTA_AB_SHA256_SIZE) == 0;
}

ota_resume_result_t ota_resume_begin(
    ota_resume_record_t *record,
    uint8_t target_slot,
    uint32_t key_id,
    uint32_t firmware_version,
    uint32_t image_size,
    uint32_t image_crc32,
    uint16_t block_count,
    const uint8_t package_id[CAN_OTA_AB_SHA256_SIZE])
{
    if (record == NULL || package_id == NULL ||
        !ota_ab_slot_id_is_valid(target_slot) ||
        block_count == 0u || block_count > CAN_OTA_AB_MAX_BLOCKS) {
        return OTA_RESUME_INVALID;
    }
    memset(record, 0, sizeof(*record));
    record->state = OTA_RESUME_STATE_RECEIVING;
    record->target_slot = target_slot;
    record->block_size = CAN_OTA_AB_BLOCK_SIZE;
    record->key_id = key_id;
    record->firmware_version = firmware_version;
    record->image_size = image_size;
    record->image_crc32 = image_crc32;
    record->block_count = block_count;
    memcpy(record->package_id, package_id, CAN_OTA_AB_SHA256_SIZE);
    return ota_resume_append(record);
}

ota_resume_result_t ota_resume_mark_block(
    ota_resume_record_t *record, uint16_t block)
{
    if (record == NULL || block != ota_resume_first_missing(record) ||
        block >= record->block_count) {
        return OTA_RESUME_INVALID;
    }
    record->completed_bitmap[block / 8u] |=
        (uint8_t)(1u << (block % 8u));
    return ota_resume_append(record);
}

ota_resume_result_t ota_resume_truncate(
    ota_resume_record_t *record, uint16_t first_missing)
{
    uint16_t block;
    if (record == NULL || first_missing > record->block_count) {
        return OTA_RESUME_INVALID;
    }
    for (block = first_missing; block < record->block_count; block++) {
        record->completed_bitmap[block / 8u] &=
            (uint8_t)~(1u << (block % 8u));
    }
    record->state = OTA_RESUME_STATE_RECEIVING;
    return ota_resume_append(record);
}

ota_resume_result_t ota_resume_mark_complete(ota_resume_record_t *record)
{
    if (record == NULL ||
        ota_resume_first_missing(record) != record->block_count) {
        return OTA_RESUME_INVALID;
    }
    record->state = OTA_RESUME_STATE_COMPLETE;
    return ota_resume_append(record);
}
