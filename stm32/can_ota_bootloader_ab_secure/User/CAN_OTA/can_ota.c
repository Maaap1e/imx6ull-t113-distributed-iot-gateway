#include "./CAN_OTA/can_ota.h"

#include <string.h>

#include "./BSP/STMFLASH/stmflash.h"
#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/usart/usart.h"
#include "../../../../common/crypto/sha256.h"
#include "../../../common_ab_secure/crypto/uECC.h"
#include "../../../common_ab_secure/ota_resume_journal.h"
#include "../../../common_ab_secure/ota_trusted_key.h"

static CAN_HandleTypeDef g_can;
static CAN_TxHeaderTypeDef g_tx_header;
static CAN_RxHeaderTypeDef g_rx_header;

static uint16_t g_write_buf[CAN_OTA_AB_WRITE_BYTES / 2u];
static uint8_t g_auth_header[CAN_OTA_AB_PACKAGE_HEADER_SIZE];
static uint8_t g_expected_sha256[CAN_OTA_AB_SHA256_SIZE];
static uint8_t g_expected_table_sha256[CAN_OTA_AB_SHA256_SIZE];
static uint8_t g_package_id[CAN_OTA_AB_SHA256_SIZE];
static uint8_t g_block_hashes[CAN_OTA_AB_BLOCK_HASH_TABLE_SIZE];
static ota_resume_record_t g_resume;
static uint16_t g_write_pos;
static uint32_t g_write_addr;
static uint32_t g_slot_end;
static uint32_t g_received_size;
static uint32_t g_expected_size;
static uint32_t g_expected_crc;
static uint32_t g_expected_version;
static uint32_t g_expected_key_id;
static uint16_t g_expected_block_count;
static uint16_t g_expected_seq;
static uint8_t g_target_slot = CAN_OTA_AB_SLOT_B;

static uint16_t le16(const uint8_t *buf)
{
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static uint32_t le32(const uint8_t *buf)
{
    return (uint32_t)buf[0] |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}

uint32_t can_ota_ab_crc32_flash(uint32_t addr, uint32_t len)
{
    return ota_ab_crc32((const void *)addr, len);
}

void can_ota_ab_sha256_flash(
    uint32_t addr, uint32_t len,
    uint8_t digest[CAN_OTA_AB_SHA256_SIZE])
{
    ota_sha256((const void *)addr, len, digest);
}

void HAL_CAN_MspInit(CAN_HandleTypeDef *hcan)
{
    GPIO_InitTypeDef gpio;

    if (hcan->Instance != CAN1) {
        return;
    }
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_CAN1_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_12;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_11;
    gpio.Mode = GPIO_MODE_AF_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &gpio);
}

static uint8_t can_send(uint32_t id, uint8_t *data, uint8_t len)
{
    uint32_t mailbox;

    g_tx_header.StdId = id;
    g_tx_header.ExtId = 0u;
    g_tx_header.IDE = CAN_ID_STD;
    g_tx_header.RTR = CAN_RTR_DATA;
    g_tx_header.DLC = len;
    if (HAL_CAN_AddTxMessage(&g_can, &g_tx_header, data, &mailbox) != HAL_OK) {
        return 1u;
    }
    while (HAL_CAN_GetTxMailboxesFreeLevel(&g_can) != 3u) {
    }
    return 0u;
}

static uint8_t can_receive(uint32_t *id, uint8_t *data)
{
    if (HAL_CAN_GetRxFifoFillLevel(&g_can, CAN_RX_FIFO0) == 0u) {
        return 0u;
    }
    if (HAL_CAN_GetRxMessage(&g_can, CAN_RX_FIFO0,
                             &g_rx_header, data) != HAL_OK) {
        return 0u;
    }
    if (g_rx_header.IDE != CAN_ID_STD ||
        g_rx_header.RTR != CAN_RTR_DATA) {
        return 0u;
    }
    *id = g_rx_header.StdId;
    return g_rx_header.DLC;
}

static void refresh_target_slot(void)
{
    ota_ab_metadata_t metadata;
    ota_ab_meta_result_t result = ota_ab_metadata_load(&metadata);

    if (result == OTA_AB_META_EMPTY) {
        (void)ota_ab_metadata_import_legacy();
        result = ota_ab_metadata_load(&metadata);
    }
    g_target_slot = result == OTA_AB_META_OK ?
                    ota_ab_metadata_choose_update_slot(&metadata) :
                    CAN_OTA_AB_SLOT_B;
}

void can_ota_ab_init(void)
{
    CAN_FilterTypeDef filter;

    g_can.Instance = CAN1;
    g_can.Init.Prescaler = 4;
    g_can.Init.Mode = CAN_MODE_NORMAL;
    g_can.Init.SyncJumpWidth = CAN_SJW_1TQ;
    g_can.Init.TimeSeg1 = CAN_BS1_9TQ;
    g_can.Init.TimeSeg2 = CAN_BS2_8TQ;
    g_can.Init.TimeTriggeredMode = DISABLE;
    g_can.Init.AutoBusOff = DISABLE;
    g_can.Init.AutoWakeUp = DISABLE;
    g_can.Init.AutoRetransmission = ENABLE;
    g_can.Init.ReceiveFifoLocked = DISABLE;
    g_can.Init.TransmitFifoPriority = DISABLE;
    HAL_CAN_Init(&g_can);

    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh = 0u;
    filter.FilterIdLow = 0u;
    filter.FilterMaskIdHigh = 0u;
    filter.FilterMaskIdLow = 0u;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = CAN_FILTER_ENABLE;
    filter.SlaveStartFilterBank = 14;
    HAL_CAN_ConfigFilter(&g_can, &filter);
    HAL_CAN_Start(&g_can);
    refresh_target_slot();
}

uint8_t can_ota_ab_target_slot(void)
{
    return g_target_slot;
}

void can_ota_ab_send_status(uint8_t status, uint8_t error,
                            uint8_t progress, uint16_t seq)
{
    uint8_t buf[8];
    uint32_t kb = g_received_size / 1024u;

    buf[0] = status;
    buf[1] = error;
    buf[2] = progress;
    buf[3] = (uint8_t)seq;
    buf[4] = (uint8_t)(seq >> 8);
    buf[5] = (uint8_t)kb;
    buf[6] = (uint8_t)(kb >> 8);
    buf[7] = g_target_slot;
    can_send(CAN_OTA_AB_ID_STATUS, buf, 8u);
}

uint8_t can_ota_ab_wait_enter(uint32_t timeout_ms)
{
    uint8_t buf[8];
    uint32_t id;
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms) {
        uint8_t len = can_receive(&id, buf);
        if (len >= 1u &&
            id == CAN_OTA_AB_ID_ENTER &&
            buf[0] == CAN_OTA_AB_CMD_ENTER) {
            refresh_target_slot();
            g_received_size = 0u;
            can_ota_ab_send_status(CAN_OTA_AB_STATUS_READY,
                                   CAN_OTA_AB_ERR_NONE, 0u, 0u);
            return 1u;
        }
        delay_ms(2u);
    }
    return 0u;
}

uint8_t can_ota_ab_app_is_valid(uint32_t app_addr, uint32_t image_size)
{
    uint32_t sp = *(volatile const uint32_t *)app_addr;
    uint32_t reset = *(volatile const uint32_t *)(app_addr + 4u);
    uint32_t reset_addr = reset & ~1u;

    if (image_size < 8u || image_size > CAN_OTA_AB_SLOT_SIZE) {
        return 0u;
    }
    if (sp < CAN_OTA_AB_SRAM_START || sp > CAN_OTA_AB_SRAM_END) {
        return 0u;
    }
    if ((reset & 1u) == 0u ||
        reset_addr < app_addr ||
        reset_addr >= app_addr + image_size) {
        return 0u;
    }
    return 1u;
}

uint8_t can_ota_ab_image_matches_slot(
    uint8_t slot, const ota_ab_slot_info_t *info)
{
    uint8_t digest[CAN_OTA_AB_SHA256_SIZE];
    uint32_t address;

    if (!ota_ab_slot_id_is_valid(slot) || info == NULL ||
        (info->flags & OTA_AB_SLOT_FLAG_VALID) == 0u) {
        return 0u;
    }
    address = ota_ab_slot_address(slot);
    if (!can_ota_ab_app_is_valid(address, info->image_size) ||
        can_ota_ab_crc32_flash(address, info->image_size) !=
            info->image_crc32) {
        return 0u;
    }
    if ((info->flags & OTA_AB_SLOT_FLAG_SIGNED) != 0u) {
        can_ota_ab_sha256_flash(address, info->image_size, digest);
        if (memcmp(digest, info->image_sha256, sizeof(digest)) != 0) {
            return 0u;
        }
    }
    return 1u;
}

static uint8_t erase_target_area(uint32_t image_size)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0u;
    uint32_t pages;

    if (image_size < 8u || image_size > CAN_OTA_AB_SLOT_SIZE) {
        return 1u;
    }
    pages = (image_size + CAN_OTA_AB_FLASH_PAGE_SIZE - 1u) /
            CAN_OTA_AB_FLASH_PAGE_SIZE;
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks = FLASH_BANK_1;
    erase.PageAddress = ota_ab_slot_address(g_target_slot);
    erase.NbPages = pages;

    HAL_FLASH_Unlock();
    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK ||
        page_error != 0xFFFFFFFFu) {
        HAL_FLASH_Lock();
        return 1u;
    }
    HAL_FLASH_Lock();
    return 0u;
}

static uint8_t erase_target_block(uint16_t block)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0u;
    uint32_t address = ota_ab_slot_address(g_target_slot) +
                       (uint32_t)block * CAN_OTA_AB_BLOCK_SIZE;

    if (block >= g_expected_block_count ||
        address + CAN_OTA_AB_BLOCK_SIZE > g_slot_end) {
        return 1u;
    }
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks = FLASH_BANK_1;
    erase.PageAddress = address;
    erase.NbPages = 1u;
    HAL_FLASH_Unlock();
    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK ||
        page_error != 0xFFFFFFFFu) {
        HAL_FLASH_Lock();
        return 1u;
    }
    HAL_FLASH_Lock();
    return 0u;
}

static uint32_t block_length(uint16_t block)
{
    uint32_t offset = (uint32_t)block * CAN_OTA_AB_BLOCK_SIZE;
    uint32_t remaining = g_expected_size - offset;
    return remaining < CAN_OTA_AB_BLOCK_SIZE ?
           remaining : CAN_OTA_AB_BLOCK_SIZE;
}

static uint8_t flash_block_matches(uint16_t block)
{
    uint8_t digest[CAN_OTA_AB_SHA256_SIZE];
    uint32_t address = ota_ab_slot_address(g_target_slot) +
                       (uint32_t)block * CAN_OTA_AB_BLOCK_SIZE;

    can_ota_ab_sha256_flash(address, block_length(block), digest);
    return memcmp(digest,
                  g_block_hashes +
                      (uint32_t)block * CAN_OTA_AB_SHA256_SIZE,
                  sizeof(digest)) == 0;
}

static uint8_t flush_write_buf(void)
{
    uint16_t block;
    uint16_t image_bytes;
    uint16_t padded_bytes;
    uint16_t index;

    if (g_write_pos == 0u) {
        return 0u;
    }
    block = (uint16_t)((g_write_addr -
                        ota_ab_slot_address(g_target_slot)) /
                       CAN_OTA_AB_BLOCK_SIZE);
    image_bytes = g_write_pos;
    if (block >= g_expected_block_count ||
        image_bytes != block_length(block)) {
        return CAN_OTA_AB_ERR_SIZE;
    }
    if ((g_write_pos & 1u) != 0u) {
        ((uint8_t *)g_write_buf)[g_write_pos++] = 0xFFu;
    }
    padded_bytes = g_write_pos;
    if (g_write_addr + padded_bytes > g_slot_end) {
        return CAN_OTA_AB_ERR_FLASH;
    }

    stmflash_write(g_write_addr, g_write_buf, g_write_pos / 2u);
    for (index = 0u; index < padded_bytes; index++) {
        if (*(volatile const uint8_t *)(g_write_addr + index) !=
            ((uint8_t *)g_write_buf)[index]) {
            return CAN_OTA_AB_ERR_FLASH;
        }
    }
    g_write_addr += padded_bytes;
    g_write_pos = 0u;
    if (!flash_block_matches(block)) {
        return CAN_OTA_AB_ERR_BLOCK_HASH;
    }
    if (ota_resume_mark_block(&g_resume, block) != OTA_RESUME_OK) {
        return CAN_OTA_AB_ERR_RESUME;
    }
    return CAN_OTA_AB_ERR_NONE;
}

static uint8_t write_image_bytes(const uint8_t *data, uint8_t len)
{
    uint8_t index;
    uint8_t *buffer = (uint8_t *)g_write_buf;

    for (index = 0u; index < len; index++) {
        uint8_t result;
        buffer[g_write_pos++] = data[index];
        if (g_write_pos == CAN_OTA_AB_WRITE_BYTES) {
            result = flush_write_buf();
            if (result != CAN_OTA_AB_ERR_NONE) {
                return result;
            }
        }
    }
    return CAN_OTA_AB_ERR_NONE;
}

static uint8_t receive_auth_header(void)
{
    uint32_t id;
    uint32_t received = 0u;
    uint16_t expected_seq = 0u;
    uint32_t last_rx_tick = HAL_GetTick();

    memset(g_auth_header, 0, sizeof(g_auth_header));
    while (received < sizeof(g_auth_header)) {
        uint8_t frame[8];
        uint8_t len = can_receive(&id, frame);

        if (len == 0u) {
            if ((HAL_GetTick() - last_rx_tick) > 5000u) {
                return CAN_OTA_AB_ERR_TIMEOUT;
            }
            continue;
        }
        if (id != CAN_OTA_AB_ID_AUTH || len != 8u) {
            continue;
        }
        last_rx_tick = HAL_GetTick();
        if (le16(frame) != expected_seq) {
            return CAN_OTA_AB_ERR_SEQ;
        }
        {
            uint32_t remaining = sizeof(g_auth_header) - received;
            uint8_t copy = remaining >= 6u ? 6u : (uint8_t)remaining;
            memcpy(g_auth_header + received, frame + 2u, copy);
            received += copy;
        }
        expected_seq++;
    }
    return CAN_OTA_AB_ERR_NONE;
}

static uint8_t receive_block_hash_table(void)
{
    uint32_t id;
    uint32_t received = 0u;
    uint32_t required =
        (uint32_t)g_expected_block_count * CAN_OTA_AB_SHA256_SIZE;
    uint16_t expected_seq = 0u;
    uint32_t last_rx_tick = HAL_GetTick();
    uint8_t digest[CAN_OTA_AB_SHA256_SIZE];

    memset(g_block_hashes, 0, sizeof(g_block_hashes));
    while (received < required) {
        uint8_t frame[8];
        uint8_t len = can_receive(&id, frame);
        if (len == 0u) {
            if ((HAL_GetTick() - last_rx_tick) > 5000u) {
                return CAN_OTA_AB_ERR_TIMEOUT;
            }
            continue;
        }
        if (id != CAN_OTA_AB_ID_BLOCK_HASH || len != 8u) {
            continue;
        }
        last_rx_tick = HAL_GetTick();
        if (le16(frame) != expected_seq) {
            return CAN_OTA_AB_ERR_SEQ;
        }
        {
            uint32_t remaining = required - received;
            uint8_t copy = remaining >= 6u ? 6u : (uint8_t)remaining;
            memcpy(g_block_hashes + received, frame + 2u, copy);
            received += copy;
        }
        expected_seq++;
    }
    ota_sha256(g_block_hashes, required, digest);
    return memcmp(digest, g_expected_table_sha256, sizeof(digest)) == 0 ?
           CAN_OTA_AB_ERR_NONE : CAN_OTA_AB_ERR_BLOCK_HASH;
}

static uint8_t validate_auth_header(void)
{
    ota_ab_metadata_t metadata;
    ota_ab_meta_result_t metadata_result;
    uint8_t manifest_digest[CAN_OTA_AB_SHA256_SIZE];
    uint32_t selected_size_offset;
    uint32_t selected_crc_offset;
    uint32_t selected_sha_offset;
    uint32_t selected_count_offset;
    uint32_t selected_table_sha_offset;
    uint32_t confirmed_version = 0u;
    uECC_Curve curve = uECC_secp256r1();

    if (le32(g_auth_header + CAN_OTA_AB_PACKAGE_OFF_MAGIC) !=
            CAN_OTA_AB_PACKAGE_MAGIC ||
        le16(g_auth_header + CAN_OTA_AB_PACKAGE_OFF_HEADER_SIZE) !=
            CAN_OTA_AB_PACKAGE_HEADER_SIZE ||
        g_auth_header[CAN_OTA_AB_PACKAGE_OFF_FORMAT] !=
            CAN_OTA_AB_PACKAGE_FORMAT_VERSION ||
        g_auth_header[CAN_OTA_AB_PACKAGE_OFF_SIGNATURE_ALG] !=
            CAN_OTA_AB_SIGNATURE_ECDSA_P256 ||
        g_auth_header[CAN_OTA_AB_PACKAGE_OFF_HASH_ALG] !=
            CAN_OTA_AB_HASH_SHA256 ||
        le32(g_auth_header + CAN_OTA_AB_PACKAGE_OFF_RESERVED) != 0u ||
        le16(g_auth_header + CAN_OTA_AB_PACKAGE_OFF_A_RESERVED) != 0u ||
        le16(g_auth_header + CAN_OTA_AB_PACKAGE_OFF_B_RESERVED) != 0u ||
        le32(g_auth_header + CAN_OTA_AB_PACKAGE_OFF_BLOCK_SIZE) !=
            CAN_OTA_AB_BLOCK_SIZE) {
        return CAN_OTA_AB_ERR_MANIFEST;
    }
    if ((g_auth_header[CAN_OTA_AB_PACKAGE_OFF_FLAGS] &
         (uint8_t)~CAN_OTA_AB_MANIFEST_KNOWN_FLAGS) != 0u) {
        return CAN_OTA_AB_ERR_MANIFEST;
    }
    if (le16(g_auth_header + CAN_OTA_AB_PACKAGE_OFF_HARDWARE_ID) !=
        CAN_OTA_HARDWARE_ID_STM32F103) {
        return CAN_OTA_AB_ERR_HARDWARE;
    }
    if (ota_ab_crc32(g_auth_header, CAN_OTA_AB_PACKAGE_CRC_BYTES) !=
        le32(g_auth_header + CAN_OTA_AB_PACKAGE_OFF_HEADER_CRC32)) {
        return CAN_OTA_AB_ERR_CRC;
    }

    g_expected_key_id =
        le32(g_auth_header + CAN_OTA_AB_PACKAGE_OFF_KEY_ID);
    if (g_ota_trusted_key_id == 0u ||
        g_expected_key_id != g_ota_trusted_key_id) {
        return CAN_OTA_AB_ERR_KEY_ID;
    }

    ota_sha256(g_auth_header, CAN_OTA_AB_PACKAGE_AUTH_BYTES,
               manifest_digest);
    memcpy(g_package_id, manifest_digest, sizeof(g_package_id));
    if (!uECC_verify(
            g_ota_trusted_public_key,
            manifest_digest,
            sizeof(manifest_digest),
            g_auth_header + CAN_OTA_AB_PACKAGE_OFF_SIGNATURE,
            curve)) {
        return CAN_OTA_AB_ERR_SIGNATURE;
    }

    g_expected_version =
        le32(g_auth_header + CAN_OTA_AB_PACKAGE_OFF_VERSION);
    if (g_expected_version == 0u) {
        return CAN_OTA_AB_ERR_MANIFEST;
    }
    metadata_result = ota_ab_metadata_load(&metadata);
    if (metadata_result == OTA_AB_META_OK &&
        ota_ab_slot_id_is_valid(metadata.confirmed_slot)) {
        if (metadata.confirmed_slot == g_target_slot) {
            return CAN_OTA_AB_ERR_SLOT;
        }
        confirmed_version =
            metadata.slots[metadata.confirmed_slot].firmware_version;
    }
    if (confirmed_version != 0u &&
        g_expected_version < confirmed_version &&
        (g_auth_header[CAN_OTA_AB_PACKAGE_OFF_FLAGS] &
         CAN_OTA_AB_MANIFEST_ALLOW_DOWNGRADE) == 0u) {
        return CAN_OTA_AB_ERR_ROLLBACK_POLICY;
    }

    if (g_target_slot == CAN_OTA_AB_SLOT_A) {
        selected_size_offset = CAN_OTA_AB_PACKAGE_OFF_A_SIZE;
        selected_crc_offset = CAN_OTA_AB_PACKAGE_OFF_A_CRC32;
        selected_sha_offset = CAN_OTA_AB_PACKAGE_OFF_A_SHA256;
        selected_count_offset = CAN_OTA_AB_PACKAGE_OFF_A_BLOCK_COUNT;
        selected_table_sha_offset =
            CAN_OTA_AB_PACKAGE_OFF_A_TABLE_SHA;
    } else {
        selected_size_offset = CAN_OTA_AB_PACKAGE_OFF_B_SIZE;
        selected_crc_offset = CAN_OTA_AB_PACKAGE_OFF_B_CRC32;
        selected_sha_offset = CAN_OTA_AB_PACKAGE_OFF_B_SHA256;
        selected_count_offset = CAN_OTA_AB_PACKAGE_OFF_B_BLOCK_COUNT;
        selected_table_sha_offset =
            CAN_OTA_AB_PACKAGE_OFF_B_TABLE_SHA;
    }
    g_expected_size = le32(g_auth_header + selected_size_offset);
    g_expected_crc = le32(g_auth_header + selected_crc_offset);
    memcpy(g_expected_sha256, g_auth_header + selected_sha_offset,
           sizeof(g_expected_sha256));
    g_expected_block_count =
        le16(g_auth_header + selected_count_offset);
    memcpy(g_expected_table_sha256,
           g_auth_header + selected_table_sha_offset,
           sizeof(g_expected_table_sha256));
    if (g_expected_size < 8u ||
        g_expected_size > CAN_OTA_AB_SLOT_SIZE ||
        g_expected_block_count == 0u ||
        g_expected_block_count > CAN_OTA_AB_MAX_BLOCKS ||
        g_expected_block_count !=
            (g_expected_size + CAN_OTA_AB_BLOCK_SIZE - 1u) /
                CAN_OTA_AB_BLOCK_SIZE) {
        return CAN_OTA_AB_ERR_SIZE;
    }
    return CAN_OTA_AB_ERR_NONE;
}

static uint8_t prepare_resumable_write(void)
{
    ota_resume_result_t load_result = ota_resume_load(&g_resume);
    uint8_t matching =
        load_result == OTA_RESUME_OK &&
        ota_resume_matches(&g_resume, g_target_slot, g_expected_key_id,
                           g_expected_version, g_expected_size,
                           g_expected_crc, g_expected_block_count,
                           g_package_id);
    uint16_t first_missing = 0u;
    uint16_t block;

    can_ota_ab_send_status(CAN_OTA_AB_STATUS_ERASING,
                           CAN_OTA_AB_ERR_NONE, 0u, 0u);
    if (!matching) {
        if (erase_target_area(g_expected_size) != 0u) {
            return CAN_OTA_AB_ERR_FLASH;
        }
        if (ota_resume_begin(&g_resume, g_target_slot, g_expected_key_id,
                             g_expected_version, g_expected_size,
                             g_expected_crc, g_expected_block_count,
                             g_package_id) != OTA_RESUME_OK) {
            return CAN_OTA_AB_ERR_RESUME;
        }
    } else {
        first_missing = ota_resume_first_missing(&g_resume);
        for (block = 0u; block < first_missing; block++) {
            if (!flash_block_matches(block)) {
                first_missing = block;
                if (ota_resume_truncate(&g_resume, first_missing) !=
                    OTA_RESUME_OK) {
                    return CAN_OTA_AB_ERR_RESUME;
                }
                break;
            }
        }
        if (first_missing < g_expected_block_count &&
            erase_target_block(first_missing) != 0u) {
            return CAN_OTA_AB_ERR_FLASH;
        }
    }

    first_missing = ota_resume_first_missing(&g_resume);
    g_received_size = first_missing == g_expected_block_count ?
                      g_expected_size :
                      (uint32_t)first_missing * CAN_OTA_AB_BLOCK_SIZE;
    g_write_addr = ota_ab_slot_address(g_target_slot) + g_received_size;
    g_write_pos = 0u;
    g_expected_seq = 0u;
    return CAN_OTA_AB_ERR_NONE;
}

uint8_t can_ota_ab_run(void)
{
    uint8_t error;
    uint8_t frame[8];
    uint8_t digest[CAN_OTA_AB_SHA256_SIZE];
    uint32_t id;
    uint32_t last_rx_tick;
    ota_ab_meta_result_t metadata_result;

    refresh_target_slot();
    error = receive_auth_header();
    if (error == CAN_OTA_AB_ERR_NONE) {
        error = validate_auth_header();
    }
    if (error != CAN_OTA_AB_ERR_NONE) {
        can_ota_ab_send_status(CAN_OTA_AB_STATUS_ERROR, error, 0u, 0u);
        return 1u;
    }

    g_slot_end = ota_ab_slot_address(g_target_slot) +
                 CAN_OTA_AB_SLOT_SIZE;
    can_ota_ab_send_status(CAN_OTA_AB_STATUS_READY,
                           CAN_OTA_AB_ERR_NONE, 0u, 0u);
    error = receive_block_hash_table();
    if (error == CAN_OTA_AB_ERR_NONE) {
        error = prepare_resumable_write();
    }
    if (error != CAN_OTA_AB_ERR_NONE) {
        can_ota_ab_send_status(CAN_OTA_AB_STATUS_ERROR,
                               error, 0u, 0u);
        return 1u;
    }
    can_ota_ab_send_status(CAN_OTA_AB_STATUS_WRITING,
                           CAN_OTA_AB_ERR_NONE,
                           (uint8_t)((g_received_size * 100u) /
                                     g_expected_size),
                           0u);

    last_rx_tick = HAL_GetTick();
    while (g_received_size < g_expected_size) {
        uint8_t len = can_receive(&id, frame);

        if (len == 0u) {
            if ((HAL_GetTick() - last_rx_tick) > 5000u) {
                can_ota_ab_send_status(
                    CAN_OTA_AB_STATUS_ERROR, CAN_OTA_AB_ERR_TIMEOUT,
                    0u, g_expected_seq);
                return 1u;
            }
            continue;
        }
        if (id == CAN_OTA_AB_ID_DATA && len >= 3u && len <= 8u) {
            uint16_t seq = le16(frame);
            uint32_t remaining = g_expected_size - g_received_size;
            uint8_t data_len = len - 2u;

            last_rx_tick = HAL_GetTick();
            if (seq != g_expected_seq) {
                can_ota_ab_send_status(
                    CAN_OTA_AB_STATUS_ERROR, CAN_OTA_AB_ERR_SEQ, 0u, seq);
                return 1u;
            }
            if (data_len > remaining) {
                can_ota_ab_send_status(
                    CAN_OTA_AB_STATUS_ERROR, CAN_OTA_AB_ERR_SIZE,
                    0u, seq);
                return 1u;
            }
            error = write_image_bytes(frame + 2u, data_len);
            if (error != CAN_OTA_AB_ERR_NONE) {
                can_ota_ab_send_status(
                    CAN_OTA_AB_STATUS_ERROR, error,
                    0u, seq);
                return 1u;
            }
            g_received_size += data_len;
            if ((g_expected_seq & 0x1Fu) == 0u ||
                (g_received_size % CAN_OTA_AB_BLOCK_SIZE) == 0u ||
                g_received_size == g_expected_size) {
                uint8_t progress =
                    (uint8_t)((g_received_size * 100u) / g_expected_size);
                can_ota_ab_send_status(
                    CAN_OTA_AB_STATUS_WRITING, CAN_OTA_AB_ERR_NONE,
                    progress, seq);
            }
            g_expected_seq++;
        }
    }

    error = flush_write_buf();
    if (error != CAN_OTA_AB_ERR_NONE) {
        can_ota_ab_send_status(CAN_OTA_AB_STATUS_ERROR,
                               error, 100u, g_expected_seq);
        return 1u;
    }
    can_ota_ab_send_status(CAN_OTA_AB_STATUS_VERIFY,
                           CAN_OTA_AB_ERR_NONE, 100u, g_expected_seq);

    if (can_ota_ab_crc32_flash(ota_ab_slot_address(g_target_slot),
                               g_expected_size) != g_expected_crc) {
        can_ota_ab_send_status(CAN_OTA_AB_STATUS_ERROR,
                               CAN_OTA_AB_ERR_CRC, 100u, g_expected_seq);
        return 1u;
    }
    can_ota_ab_sha256_flash(ota_ab_slot_address(g_target_slot),
                            g_expected_size, digest);
    if (memcmp(digest, g_expected_sha256, sizeof(digest)) != 0) {
        can_ota_ab_send_status(CAN_OTA_AB_STATUS_ERROR,
                               CAN_OTA_AB_ERR_SHA256, 100u, g_expected_seq);
        return 1u;
    }
    if (!can_ota_ab_app_is_valid(ota_ab_slot_address(g_target_slot),
                                 g_expected_size)) {
        can_ota_ab_send_status(CAN_OTA_AB_STATUS_ERROR,
                               CAN_OTA_AB_ERR_APP, 100u, g_expected_seq);
        return 1u;
    }
    if (g_resume.state != OTA_RESUME_STATE_COMPLETE &&
        ota_resume_mark_complete(&g_resume) != OTA_RESUME_OK) {
        can_ota_ab_send_status(CAN_OTA_AB_STATUS_ERROR,
                               CAN_OTA_AB_ERR_RESUME,
                               100u, g_expected_seq);
        return 1u;
    }

    metadata_result = ota_ab_metadata_set_candidate(
        g_target_slot, g_expected_version, g_expected_size,
        g_expected_crc, g_expected_sha256, g_expected_key_id);
    if (metadata_result != OTA_AB_META_OK) {
        can_ota_ab_send_status(CAN_OTA_AB_STATUS_ERROR,
                               CAN_OTA_AB_ERR_METADATA,
                               100u, g_expected_seq);
        return 1u;
    }
    can_ota_ab_send_status(CAN_OTA_AB_STATUS_DONE,
                           CAN_OTA_AB_ERR_NONE, 100u, g_expected_seq);
    return 0u;
}
