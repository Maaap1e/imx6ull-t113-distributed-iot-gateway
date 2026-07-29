#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/KEY/key.h"
#include "./IAP/iap.h"
#include "./CAN_OTA/can_ota.h"

#define OTA_ENTER_WAIT_MS       3000u

static void boot_ui_show(void)
{
    lcd_show_string(30,  50, 220, 16, 16, "STM32F103", RED);
    lcd_show_string(30,  70, 220, 16, 16, "CAN OTA Bootloader", RED);
    lcd_show_string(30,  90, 220, 16, 16, "APP: 0x08010000", RED);
    lcd_show_string(30, 110, 220, 16, 16, "Waiting CAN OTA...", BLUE);
}

static void jump_to_app_if_valid(void)
{
    const ota_boot_metadata_t *metadata = ota_metadata_get();
    ota_metadata_result_t result;
    uint16_t image_state;

    if (ota_metadata_is_blank()) {
        printf("OTA metadata is empty; recovery mode required.\r\n");
        return;
    }

    if (!ota_metadata_is_valid(metadata)) {
        printf("OTA metadata is invalid; recovery mode required.\r\n");
        return;
    }

    image_state = ota_metadata_current_state(metadata);
    if (image_state == 0u) {
        printf("OTA state markers are invalid; recovery mode required.\r\n");
        return;
    }

    printf("Metadata version: %u.%u.%u.%u state=0x%04X size=%lu\r\n",
           CAN_OTA_VERSION_MAJOR(metadata->firmware_version),
           CAN_OTA_VERSION_MINOR(metadata->firmware_version),
           CAN_OTA_VERSION_PATCH(metadata->firmware_version),
           CAN_OTA_VERSION_BUILD(metadata->firmware_version),
           image_state,
           (unsigned long)metadata->image_size);

    if (image_state == OTA_IMAGE_STATE_TRIAL) {
        printf("Trial image was not confirmed; stay in recovery mode.\r\n");
        return;
    }

    if (!can_ota_image_matches_metadata(metadata)) {
        printf("App CRC/vector does not match OTA metadata.\r\n");
        return;
    }

    if (image_state == OTA_IMAGE_STATE_PENDING) {
        result = ota_metadata_mark_trial();
        if (result != OTA_METADATA_RESULT_OK) {
            printf("Failed to mark image as trial: %u\r\n", result);
            return;
        }
        printf("Image marked as trial boot.\r\n");
    }

    printf("Verified App, jump to 0x%08X\r\n", FLASH_APP1_ADDR);
    lcd_show_string(30, 130, 220, 16, 16, "Jump to APP...", BLUE);
    delay_ms(100);
    iap_load_app(FLASH_APP1_ADDR);
}

int main(void)
{
    uint8_t ota_started;

    HAL_Init();
    sys_stm32_clock_init(RCC_PLL_MUL9);
    delay_init(72);
    usart_init(115200);
    led_init();
    lcd_init();
    key_init();
    can_ota_init();

    boot_ui_show();
    printf("\r\nSTM32F103 CAN OTA Bootloader\r\n");
    printf("App address: 0x%08X\r\n", FLASH_APP1_ADDR);
    printf("Waiting %lu ms for CAN OTA enter command...\r\n", (unsigned long)OTA_ENTER_WAIT_MS);

    ota_started = can_ota_wait_enter(OTA_ENTER_WAIT_MS);
    if (!ota_started) {
        printf("No OTA command.\r\n");
        jump_to_app_if_valid();

        printf("No bootable App, enter recovery mode.\r\n");
        lcd_show_string(30, 130, 220, 16, 16, "OTA Recovery", BLUE);
        while (!can_ota_wait_enter(1000u)) {
            LED0_TOGGLE();
        }
    }

    while (1) {
        printf("CAN OTA started.\r\n");
        lcd_show_string(30, 130, 220, 16, 16, "OTA Running...", BLUE);

        if (can_ota_run() == 0) {
            printf("OTA success, start trial App.\r\n");
            lcd_show_string(30, 150, 220, 16, 16, "OTA Success!", BLUE);
            delay_ms(300);
            jump_to_app_if_valid();
        } else {
            printf("OTA failed, stay in recovery mode.\r\n");
            lcd_show_string(30, 150, 220, 16, 16, "OTA Failed!", BLUE);
        }

        printf("Waiting for another OTA enter command.\r\n");
        while (!can_ota_wait_enter(1000u)) {
            LED0_TOGGLE();
        }
    }
}
