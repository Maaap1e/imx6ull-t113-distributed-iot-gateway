#ifndef __CAN_OTA_H
#define __CAN_OTA_H

#include "./SYSTEM/sys/sys.h"

#define CAN_OTA_ID_ENTER        0x300u
#define CAN_OTA_ID_INFO         0x301u
#define CAN_OTA_ID_DATA         0x302u
#define CAN_OTA_ID_STATUS       0x380u

#define CAN_OTA_CMD_ENTER       0xA5u

#define CAN_OTA_STATUS_READY    0x01u
#define CAN_OTA_STATUS_ERASING  0x02u
#define CAN_OTA_STATUS_WRITING  0x03u
#define CAN_OTA_STATUS_VERIFY   0x04u
#define CAN_OTA_STATUS_DONE     0x05u
#define CAN_OTA_STATUS_ERROR    0xE0u

#define CAN_OTA_ERR_NONE        0x00u
#define CAN_OTA_ERR_TIMEOUT     0x01u
#define CAN_OTA_ERR_INFO        0x02u
#define CAN_OTA_ERR_SIZE        0x03u
#define CAN_OTA_ERR_SEQ         0x04u
#define CAN_OTA_ERR_FLASH       0x05u
#define CAN_OTA_ERR_CRC         0x06u
#define CAN_OTA_ERR_APP         0x07u

#define CAN_OTA_MAX_APP_SIZE    (448u * 1024u)
#define CAN_OTA_WRITE_BYTES     2048u

void can_ota_init(void);
uint8_t can_ota_wait_enter(uint32_t timeout_ms);
uint8_t can_ota_run(void);
uint8_t can_ota_app_is_valid(uint32_t app_addr);
void can_ota_send_status(uint8_t status, uint8_t error, uint8_t progress, uint16_t seq);

#endif
