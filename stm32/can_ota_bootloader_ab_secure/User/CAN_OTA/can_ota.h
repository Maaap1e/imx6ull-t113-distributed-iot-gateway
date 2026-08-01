#ifndef CAN_OTA_AB_SECURE_H
#define CAN_OTA_AB_SECURE_H

#include "./SYSTEM/sys/sys.h"
#include "../../../../common/ota_contract_ab.h"
#include "../../../common_ab_secure/ota_ab_metadata.h"

#define CAN_OTA_AB_WRITE_BYTES CAN_OTA_AB_BLOCK_SIZE

void can_ota_ab_init(void);
uint8_t can_ota_ab_wait_enter(uint32_t timeout_ms);
uint8_t can_ota_ab_run(void);
uint8_t can_ota_ab_target_slot(void);
uint8_t can_ota_ab_app_is_valid(uint32_t app_addr, uint32_t image_size);
uint8_t can_ota_ab_image_matches_slot(
    uint8_t slot, const ota_ab_slot_info_t *info);
uint32_t can_ota_ab_crc32_flash(uint32_t addr, uint32_t len);
void can_ota_ab_sha256_flash(
    uint32_t addr, uint32_t len,
    uint8_t digest[CAN_OTA_AB_SHA256_SIZE]);
void can_ota_ab_send_status(uint8_t status, uint8_t error,
                            uint8_t progress, uint16_t seq);

#endif
