#ifndef OTA_RESUME_JOURNAL_H
#define OTA_RESUME_JOURNAL_H

#include <stdint.h>

#include "../../common/ota_contract_ab.h"

#define OTA_RESUME_MAGIC             0x33534D52u /* "RMS3" */
#define OTA_RESUME_FORMAT            3u
#define OTA_RESUME_RECORD_SIZE       128u
#define OTA_RESUME_STATE_RECEIVING   1u
#define OTA_RESUME_STATE_COMPLETE    2u
#define OTA_RESUME_BITMAP_BYTES      16u

typedef struct {
    uint32_t magic;
    uint16_t format_version;
    uint16_t record_size;
    uint32_t sequence;
    uint8_t state;
    uint8_t target_slot;
    uint16_t block_size;
    uint32_t key_id;
    uint32_t firmware_version;
    uint32_t image_size;
    uint32_t image_crc32;
    uint16_t block_count;
    uint16_t reserved0;
    uint8_t package_id[CAN_OTA_AB_SHA256_SIZE];
    uint8_t completed_bitmap[OTA_RESUME_BITMAP_BYTES];
    uint8_t reserved[40];
    uint32_t record_crc32;
} ota_resume_record_t;

typedef enum {
    OTA_RESUME_OK = 0,
    OTA_RESUME_EMPTY = 1,
    OTA_RESUME_INVALID = 2,
    OTA_RESUME_FLASH = 3
} ota_resume_result_t;

uint8_t ota_resume_record_is_valid(const ota_resume_record_t *record);
ota_resume_result_t ota_resume_load(ota_resume_record_t *record);
ota_resume_result_t ota_resume_append(const ota_resume_record_t *record);
uint16_t ota_resume_first_missing(const ota_resume_record_t *record);
uint8_t ota_resume_matches(
    const ota_resume_record_t *record,
    uint8_t target_slot,
    uint32_t key_id,
    uint32_t firmware_version,
    uint32_t image_size,
    uint32_t image_crc32,
    uint16_t block_count,
    const uint8_t package_id[CAN_OTA_AB_SHA256_SIZE]);
ota_resume_result_t ota_resume_begin(
    ota_resume_record_t *record,
    uint8_t target_slot,
    uint32_t key_id,
    uint32_t firmware_version,
    uint32_t image_size,
    uint32_t image_crc32,
    uint16_t block_count,
    const uint8_t package_id[CAN_OTA_AB_SHA256_SIZE]);
ota_resume_result_t ota_resume_mark_block(
    ota_resume_record_t *record, uint16_t block);
ota_resume_result_t ota_resume_truncate(
    ota_resume_record_t *record, uint16_t first_missing);
ota_resume_result_t ota_resume_mark_complete(ota_resume_record_t *record);

#endif
