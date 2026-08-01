#ifndef OTA_AB_METADATA_H
#define OTA_AB_METADATA_H

#include <stdint.h>

#include "../../common/ota_contract_ab.h"

#define OTA_AB_METADATA_MAGIC          0x3242544Fu /* "OTB2" */
#define OTA_AB_METADATA_FORMAT         2u
#define OTA_AB_METADATA_RECORD_SIZE    128u

#define OTA_AB_SLOT_FLAG_VALID         0x00000001u
#define OTA_AB_SLOT_FLAG_SIGNED        0x00000002u
#define OTA_AB_SLOT_FLAG_LEGACY        0x00000004u

typedef struct {
    uint32_t firmware_version;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t flags;
    uint8_t image_sha256[CAN_OTA_AB_SHA256_SIZE];
} ota_ab_slot_info_t;

typedef struct {
    uint32_t magic;
    uint16_t format_version;
    uint16_t record_size;
    uint32_t sequence;
    uint8_t active_slot;
    uint8_t confirmed_slot;
    uint8_t candidate_slot;
    uint8_t trial_booted;
    ota_ab_slot_info_t slots[2];
    uint32_t key_id;
    uint8_t reserved[8];
    uint32_t record_crc32;
} ota_ab_metadata_t;

typedef enum {
    OTA_AB_META_OK = 0,
    OTA_AB_META_EMPTY = 1,
    OTA_AB_META_INVALID = 2,
    OTA_AB_META_STATE = 3,
    OTA_AB_META_FLASH = 4
} ota_ab_meta_result_t;

uint32_t ota_ab_slot_address(uint8_t slot);
uint8_t ota_ab_other_slot(uint8_t slot);
uint8_t ota_ab_slot_id_is_valid(uint8_t slot);

uint32_t ota_ab_crc32(const void *data, uint32_t len);
uint8_t ota_ab_metadata_record_is_valid(const ota_ab_metadata_t *record);
ota_ab_meta_result_t ota_ab_metadata_load(ota_ab_metadata_t *record);
ota_ab_meta_result_t ota_ab_metadata_append(const ota_ab_metadata_t *record);

ota_ab_meta_result_t ota_ab_metadata_import_legacy(void);
uint8_t ota_ab_metadata_choose_update_slot(const ota_ab_metadata_t *record);
ota_ab_meta_result_t ota_ab_metadata_set_candidate(
    uint8_t slot,
    uint32_t firmware_version,
    uint32_t image_size,
    uint32_t image_crc32,
    const uint8_t image_sha256[CAN_OTA_AB_SHA256_SIZE],
    uint32_t key_id);
ota_ab_meta_result_t ota_ab_metadata_mark_trial(uint8_t slot);
ota_ab_meta_result_t ota_ab_metadata_confirm(uint8_t slot,
                                             uint32_t firmware_version);
ota_ab_meta_result_t ota_ab_metadata_rollback(void);

#endif
