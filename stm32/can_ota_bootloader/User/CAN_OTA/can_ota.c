#include "./CAN_OTA/can_ota.h"
#include "./BSP/STMFLASH/stmflash.h"
#include "./IAP/iap.h"
#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/usart/usart.h"

static CAN_HandleTypeDef g_can;
static CAN_TxHeaderTypeDef g_tx_header;
static CAN_RxHeaderTypeDef g_rx_header;

static uint16_t g_write_buf[CAN_OTA_WRITE_BYTES / 2u];
static uint16_t g_write_pos;
static uint32_t g_write_addr;
static uint32_t g_received_size;
static uint32_t g_expected_size;
static uint32_t g_expected_crc;
static uint16_t g_expected_seq;

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

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    uint32_t bit;

    crc = ~crc;
    while (len--) {
        crc ^= *data++;
        for (bit = 0; bit < 8; bit++) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }

    return ~crc;
}

static uint32_t crc32_flash(uint32_t addr, uint32_t len)
{
    uint32_t crc = 0;
    uint32_t i;

    for (i = 0; i < len; i++) {
        uint8_t value = *(volatile uint8_t *)(addr + i);
        crc = crc32_update(crc, &value, 1);
    }

    return crc;
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
    g_tx_header.ExtId = 0;
    g_tx_header.IDE = CAN_ID_STD;
    g_tx_header.RTR = CAN_RTR_DATA;
    g_tx_header.DLC = len;

    if (HAL_CAN_AddTxMessage(&g_can, &g_tx_header, data, &mailbox) != HAL_OK) {
        return 1;
    }

    while (HAL_CAN_GetTxMailboxesFreeLevel(&g_can) != 3) {
    }

    return 0;
}

static uint8_t can_receive(uint32_t *id, uint8_t *data)
{
    if (HAL_CAN_GetRxFifoFillLevel(&g_can, CAN_RX_FIFO0) == 0) {
        return 0;
    }

    if (HAL_CAN_GetRxMessage(&g_can, CAN_RX_FIFO0, &g_rx_header, data) != HAL_OK) {
        return 0;
    }

    if (g_rx_header.IDE != CAN_ID_STD || g_rx_header.RTR != CAN_RTR_DATA) {
        return 0;
    }

    *id = g_rx_header.StdId;
    return g_rx_header.DLC;
}

void can_ota_init(void)
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
    filter.FilterIdHigh = 0x0000;
    filter.FilterIdLow = 0x0000;
    filter.FilterMaskIdHigh = 0x0000;
    filter.FilterMaskIdLow = 0x0000;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = CAN_FILTER_ENABLE;
    filter.SlaveStartFilterBank = 14;
    HAL_CAN_ConfigFilter(&g_can, &filter);
    HAL_CAN_Start(&g_can);
}

void can_ota_send_status(uint8_t status, uint8_t error, uint8_t progress, uint16_t seq)
{
    uint8_t buf[8];
    uint32_t kb = g_received_size / 1024u;

    buf[0] = status;
    buf[1] = error;
    buf[2] = progress;
    buf[3] = (uint8_t)(seq & 0xFFu);
    buf[4] = (uint8_t)(seq >> 8);
    buf[5] = (uint8_t)(kb & 0xFFu);
    buf[6] = (uint8_t)((kb >> 8) & 0xFFu);
    buf[7] = 0;
    can_send(CAN_OTA_ID_STATUS, buf, 8);
}

uint8_t can_ota_wait_enter(uint32_t timeout_ms)
{
    uint8_t buf[8];
    uint32_t id;
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms) {
        uint8_t len = can_receive(&id, buf);
        if (len >= 1 && id == CAN_OTA_ID_ENTER && buf[0] == CAN_OTA_CMD_ENTER) {
            can_ota_send_status(CAN_OTA_STATUS_READY, CAN_OTA_ERR_NONE, 0, 0);
            return 1;
        }
        delay_ms(2);
    }

    return 0;
}

uint8_t can_ota_app_is_valid(uint32_t app_addr)
{
    uint32_t sp = *(volatile uint32_t *)app_addr;
    uint32_t reset = *(volatile uint32_t *)(app_addr + 4u);

    if ((sp & 0x2FFE0000u) != 0x20000000u) {
        return 0;
    }
    if ((reset & 0xFF000000u) != 0x08000000u) {
        return 0;
    }

    return 1;
}

static uint8_t erase_app_area(uint32_t app_size)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0;
    uint32_t pages;

    if (app_size == 0 || app_size > CAN_OTA_MAX_APP_SIZE) {
        return 1;
    }

    pages = (app_size + STM32_SECTOR_SIZE - 1u) / STM32_SECTOR_SIZE;
    HAL_FLASH_Unlock();
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks = FLASH_BANK_1;
    erase.PageAddress = FLASH_APP1_ADDR;
    erase.NbPages = pages;

    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK) {
        HAL_FLASH_Lock();
        return 1;
    }

    HAL_FLASH_Lock();
    return 0;
}

static uint8_t flush_write_buf(void)
{
    if (g_write_pos == 0) {
        return 0;
    }

    if (g_write_pos & 1u) {
        ((uint8_t *)g_write_buf)[g_write_pos++] = 0xFF;
    }

    stmflash_write(g_write_addr, g_write_buf, (g_write_pos + 1u) / 2u);
    g_write_addr += g_write_pos;
    g_write_pos = 0;
    return 0;
}

static uint8_t write_app_bytes(const uint8_t *data, uint8_t len)
{
    uint8_t i;
    uint8_t *byte_buf = (uint8_t *)g_write_buf;

    for (i = 0; i < len; i++) {
        byte_buf[g_write_pos++] = data[i];
        if (g_write_pos >= CAN_OTA_WRITE_BYTES) {
            if (flush_write_buf()) {
                return 1;
            }
        }
    }

    return 0;
}

static uint8_t wait_firmware_info(uint32_t *size, uint32_t *crc32)
{
    uint8_t buf[8];
    uint32_t id;
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < 5000u) {
        uint8_t len = can_receive(&id, buf);
        if (len == 8 && id == CAN_OTA_ID_INFO) {
            *size = le32(&buf[0]);
            *crc32 = le32(&buf[4]);
            return 0;
        }
        delay_ms(2);
    }

    return 1;
}

uint8_t can_ota_run(void)
{
    uint8_t buf[8];
    uint32_t id;
    uint32_t crc;
    uint32_t last_rx_tick;

    if (wait_firmware_info(&g_expected_size, &g_expected_crc)) {
        can_ota_send_status(CAN_OTA_STATUS_ERROR, CAN_OTA_ERR_INFO, 0, 0);
        return 1;
    }

    if (g_expected_size == 0 || g_expected_size > CAN_OTA_MAX_APP_SIZE) {
        can_ota_send_status(CAN_OTA_STATUS_ERROR, CAN_OTA_ERR_SIZE, 0, 0);
        return 1;
    }

    g_received_size = 0;
    g_expected_seq = 0;
    g_write_addr = FLASH_APP1_ADDR;
    g_write_pos = 0;

    can_ota_send_status(CAN_OTA_STATUS_ERASING, CAN_OTA_ERR_NONE, 0, 0);
    if (erase_app_area(g_expected_size)) {
        can_ota_send_status(CAN_OTA_STATUS_ERROR, CAN_OTA_ERR_FLASH, 0, 0);
        return 1;
    }

    can_ota_send_status(CAN_OTA_STATUS_WRITING, CAN_OTA_ERR_NONE, 0, 0);
    last_rx_tick = HAL_GetTick();
    while (g_received_size < g_expected_size) {
        uint8_t len = can_receive(&id, buf);

        if (len == 0) {
            if ((HAL_GetTick() - last_rx_tick) > 5000u) {
                can_ota_send_status(CAN_OTA_STATUS_ERROR, CAN_OTA_ERR_TIMEOUT, 0, g_expected_seq);
                return 1;
            }
            continue;
        }

        if (id == CAN_OTA_ID_DATA && len == 8) {
            uint16_t seq = le16(&buf[0]);
            uint8_t data_len;

            last_rx_tick = HAL_GetTick();

            if (seq != g_expected_seq) {
                can_ota_send_status(CAN_OTA_STATUS_ERROR, CAN_OTA_ERR_SEQ, 0, seq);
                return 1;
            }

            data_len = (g_expected_size - g_received_size) >= 6u ? 6u : (uint8_t)(g_expected_size - g_received_size);
            if (write_app_bytes(&buf[2], data_len)) {
                can_ota_send_status(CAN_OTA_STATUS_ERROR, CAN_OTA_ERR_FLASH, 0, seq);
                return 1;
            }

            g_received_size += data_len;
            if ((g_expected_seq & 0x1Fu) == 0 || g_received_size == g_expected_size) {
                uint8_t progress = (uint8_t)((g_received_size * 100u) / g_expected_size);
                can_ota_send_status(CAN_OTA_STATUS_WRITING, CAN_OTA_ERR_NONE, progress, seq);
            }
            g_expected_seq++;
        }
    }

    flush_write_buf();
    can_ota_send_status(CAN_OTA_STATUS_VERIFY, CAN_OTA_ERR_NONE, 100, g_expected_seq);

    crc = crc32_flash(FLASH_APP1_ADDR, g_expected_size);
    if (crc != g_expected_crc) {
        can_ota_send_status(CAN_OTA_STATUS_ERROR, CAN_OTA_ERR_CRC, 100, g_expected_seq);
        return 1;
    }

    if (!can_ota_app_is_valid(FLASH_APP1_ADDR)) {
        can_ota_send_status(CAN_OTA_STATUS_ERROR, CAN_OTA_ERR_APP, 100, g_expected_seq);
        return 1;
    }

    can_ota_send_status(CAN_OTA_STATUS_DONE, CAN_OTA_ERR_NONE, 100, g_expected_seq);
    return 0;
}
