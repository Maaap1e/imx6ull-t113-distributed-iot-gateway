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
    if (can_ota_app_is_valid(FLASH_APP1_ADDR)) {
        printf("Valid App found, jump to 0x%08X\r\n", FLASH_APP1_ADDR);
        lcd_show_string(30, 130, 220, 16, 16, "Jump to APP...", BLUE);
        delay_ms(100);
        iap_load_app(FLASH_APP1_ADDR);
    }
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

        printf("No valid App, stay in bootloader.\r\n");
        lcd_show_string(30, 130, 220, 16, 16, "No valid APP!", BLUE);
        while (!can_ota_wait_enter(1000u)) {
            LED0_TOGGLE();
        }
    }

    printf("CAN OTA started.\r\n");
    lcd_show_string(30, 130, 220, 16, 16, "OTA Running...", BLUE);

    if (can_ota_run() == 0) {
        printf("OTA success, jump to App.\r\n");
        lcd_show_string(30, 150, 220, 16, 16, "OTA Success!", BLUE);
        delay_ms(300);
        iap_load_app(FLASH_APP1_ADDR);
    }

    printf("OTA failed, stay in bootloader.\r\n");
    lcd_show_string(30, 150, 220, 16, 16, "OTA Failed!", BLUE);
    while (1) {
        LED0_TOGGLE();
        delay_ms(300);
    }
}
