#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/DHT11/dht11.h"
#include "./BSP/CAN/can.h"

#define APP_VECTOR_OFFSET       0x00010000u

#define CAN_ID_STM32_HEARTBEAT  0x101u
#define CAN_ID_STM32_DHT11      0x102u
#define CAN_ID_IMX6ULL_CONTROL  0x201u
#define CAN_ID_STM32_ACK        0x202u

#define CAN_ID_OTA_ENTER        0x300u
#define CAN_CMD_OTA_ENTER       0xA5u

#define APP_VERSION_MAJOR       1u
#define APP_VERSION_MINOR       1u

#define CTRL_CMD_LED0           0x01u
#define CTRL_CMD_LED1           0x02u
#define CTRL_CMD_ENTER_BOOT     0xA5u

static uint8_t checksum8(const uint8_t *data, uint8_t len)
{
    uint8_t i;
    uint8_t sum = 0;

    for (i = 0; i < len; i++) {
        sum += data[i];
    }

    return sum;
}

static void send_heartbeat(uint16_t counter)
{
    uint8_t frame[8];

    frame[0] = APP_VERSION_MAJOR;
    frame[1] = APP_VERSION_MINOR;
    frame[2] = (uint8_t)(counter & 0xFFu);
    frame[3] = (uint8_t)(counter >> 8);
    frame[4] = 0;
    frame[5] = 0;
    frame[6] = 0;
    frame[7] = checksum8(frame, 7);

    can_send_msg(CAN_ID_STM32_HEARTBEAT, frame, 8);
}

static void send_dht11_data(uint8_t temperature, uint8_t humidity,
                            uint8_t dht_ok, uint16_t counter)
{
    uint8_t frame[8];

    frame[0] = temperature;
    frame[1] = 0;
    frame[2] = humidity;
    frame[3] = 0;
    frame[4] = dht_ok;
    frame[5] = APP_VERSION_MAJOR;
    frame[6] = (uint8_t)(counter & 0xFFu);
    frame[7] = checksum8(frame, 7);

    can_send_msg(CAN_ID_STM32_DHT11, frame, 8);
}

static void send_ack(uint8_t cmd, uint8_t result)
{
    uint8_t frame[8];

    frame[0] = cmd;
    frame[1] = result;
    frame[2] = APP_VERSION_MAJOR;
    frame[3] = APP_VERSION_MINOR;
    frame[4] = 0;
    frame[5] = 0;
    frame[6] = 0;
    frame[7] = checksum8(frame, 7);

    can_send_msg(CAN_ID_STM32_ACK, frame, 8);
}

static void request_bootloader(void)
{
    uint8_t frame[8] = { CAN_CMD_OTA_ENTER, 0, 0, 0, 0, 0, 0, 0 };

    can_send_msg(CAN_ID_OTA_ENTER, frame, 8);
    delay_ms(20);
    NVIC_SystemReset();
}

static void handle_control(void)
{
    uint8_t frame[8];
    uint8_t len;

    len = can_receive_msg(CAN_ID_IMX6ULL_CONTROL, frame);
    if (len == 0) {
        return;
    }

    switch (frame[0]) {
    case CTRL_CMD_LED0:
        if (frame[1]) {
            LED0(0);
        } else {
            LED0(1);
        }
        send_ack(frame[0], 0);
        break;

    case CTRL_CMD_LED1:
        if (frame[1]) {
            LED1(0);
        } else {
            LED1(1);
        }
        send_ack(frame[0], 0);
        break;

    case CTRL_CMD_ENTER_BOOT:
        send_ack(frame[0], 0);
        request_bootloader();
        break;

    default:
        send_ack(frame[0], 1);
        break;
    }
}

int main(void)
{
    uint8_t temperature = 0;
    uint8_t humidity = 0;
    uint8_t dht_ok = 0;
    uint16_t counter = 0;
    uint16_t tick_10ms = 0;

    SCB->VTOR = FLASH_BASE | APP_VECTOR_OFFSET;

    HAL_Init();
    sys_stm32_clock_init(RCC_PLL_MUL9);
    delay_init(72);
    usart_init(115200);
    led_init();
    lcd_init();

    lcd_show_string(30,  50, 220, 16, 16, "STM32 DHT11 CAN APP", RED);
    lcd_show_string(30,  70, 220, 16, 16, "APP @ 0x08010000", RED);
    lcd_show_string(30,  90, 220, 16, 16, "CAN: 500K ID 0x102", RED);
    lcd_show_string(30, 130, 220, 16, 16, "Temp:  C", BLUE);
    lcd_show_string(30, 150, 220, 16, 16, "Humi:  %", BLUE);

    can_init(CAN_SJW_1TQ, CAN_BS2_8TQ, CAN_BS1_9TQ, 4, CAN_MODE_NORMAL);

    while (dht11_init()) {
        dht_ok = 0;
        lcd_show_string(30, 110, 220, 16, 16, "DHT11 Error", RED);
        send_dht11_data(0, 0, dht_ok, counter++);
        delay_ms(500);
        LED0_TOGGLE();
    }

    dht_ok = 1;
    lcd_show_string(30, 110, 220, 16, 16, "DHT11 OK   ", RED);
    send_heartbeat(counter);

    while (1) {
        handle_control();

        if ((tick_10ms % 100u) == 0u) {
            if (dht11_read_data(&temperature, &humidity) == 0) {
                dht_ok = 1;
            } else {
                dht_ok = 0;
            }

            lcd_show_num(30 + 40, 130, temperature, 2, 16, BLUE);
            lcd_show_num(30 + 40, 150, humidity, 2, 16, BLUE);

            send_dht11_data(temperature, humidity, dht_ok, counter);
            send_heartbeat(counter);
            counter++;
        }

        delay_ms(10);
        tick_10ms++;

        if ((tick_10ms % 50u) == 0u) {
            LED0_TOGGLE();
        }
    }
}
