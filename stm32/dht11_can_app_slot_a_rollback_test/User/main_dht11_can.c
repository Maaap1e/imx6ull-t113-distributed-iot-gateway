#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/DHT11/dht11.h"
#include "./BSP/CAN/can.h"
#include "../../../common/ota_contract_ab.h"
#include "../../common_ab_secure/ota_ab_metadata.h"

#define OTA_AB_APP_SLOT         CAN_OTA_AB_SLOT_A
#define OTA_AB_APP_ADDRESS      CAN_OTA_AB_SLOT_A_ADDRESS
#define APP_VECTOR_OFFSET       (OTA_AB_APP_ADDRESS - FLASH_BASE)

#define CAN_ID_STM32_DHT11      0x102u
#define CAN_ID_STM32_ACK        0x202u

#define APP_VERSION_MAJOR       2u
#define APP_VERSION_MINOR       1u
#define APP_VERSION_PATCH       0u
#define APP_VERSION_BUILD       2u

#define DHT_SAMPLE_INTERVAL_TICKS 200u

/*
 * Acceptance-test hook. Keep at 0 for release builds. Set to a nonzero
 * duration, rebuild the App, then reset during the delay to verify that an
 * unconfirmed TRIAL image is not booted repeatedly.
 */
#ifndef OTA_CONFIRM_DELAY_MS
#define OTA_CONFIRM_DELAY_MS    0u
#endif

#define APP_FIRMWARE_VERSION \
    CAN_OTA_VERSION(APP_VERSION_MAJOR, APP_VERSION_MINOR, \
                    APP_VERSION_PATCH, APP_VERSION_BUILD)

#define CTRL_CMD_LED0           0x01u
#define CTRL_CMD_LED1           0x02u
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
    frame[4] = APP_VERSION_PATCH;
    frame[5] = APP_VERSION_BUILD;
    frame[6] = OTA_AB_APP_SLOT;
    frame[7] = checksum8(frame, 7);

    can_send_msg(CAN_APP_ID_HEARTBEAT, frame, 8);
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
    uint8_t frame[8] = { CAN_OTA_AB_CMD_ENTER, 0, 0, 0, 0, 0, 0, 0 };

    can_send_msg(CAN_OTA_AB_ID_ENTER, frame, 8);
    delay_ms(20);
    NVIC_SystemReset();
}

static uint8_t read_valid_dht11_sample(uint8_t *temperature,
                                       uint8_t *humidity)
{
    uint8_t sampled_temperature = 0xFFu;
    uint8_t sampled_humidity = 0xFFu;

    /*
     * The vendor DHT11 routine returns success even when its checksum does
     * not match, leaving the output arguments unchanged. Sentinels detect
     * that false-success path. The documented sensor ranges also reject the
     * all-zero sample sometimes observed immediately after a power cycle.
     */
    if (dht11_read_data(&sampled_temperature, &sampled_humidity) != 0u ||
        sampled_temperature == 0xFFu || sampled_humidity == 0xFFu ||
        sampled_temperature > 60u ||
        sampled_humidity < 5u || sampled_humidity > 95u) {
        return 1u;
    }

    *temperature = sampled_temperature;
    *humidity = sampled_humidity;
    return 0u;
}

static void handle_control(void)
{
    uint8_t frame[8];
    uint8_t len;

    len = can_receive_msg(CAN_APP_ID_CONTROL, frame);
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

    case CAN_APP_CMD_ENTER_BOOT:
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
    uint8_t dht_ready = 0;
    uint16_t counter = 0;
    /*
     * Start at one so the first DHT11 sample is delayed by about two seconds.
     * dht11_init() already performs a sensor transaction; immediately issuing
     * dht11_read_data() can violate the DHT11 minimum sampling interval.
     */
    uint16_t tick_10ms = 1;
    ota_ab_meta_result_t confirm_result;

    SCB->VTOR = FLASH_BASE | APP_VECTOR_OFFSET;

    HAL_Init();
    sys_stm32_clock_init(RCC_PLL_MUL9);
    delay_init(72);
    usart_init(115200);
    led_init();
    lcd_init();

    lcd_show_string(30,  50, 220, 16, 16, "STM32 DHT11 CAN APP", RED);
    lcd_show_string(30,  70, 220, 16, 16, "APP Secure Slot A", RED);
    lcd_show_string(30,  90, 220, 16, 16, "CAN: 500K ID 0x102", RED);
    lcd_show_string(30, 130, 220, 16, 16, "Temp:  C", BLUE);
    lcd_show_string(30, 150, 220, 16, 16, "Humi:  %", BLUE);

    if (can_init(CAN_SJW_1TQ, CAN_BS2_8TQ, CAN_BS1_9TQ,
                 4, CAN_MODE_NORMAL) != 0u) {
        printf("CAN initialization failed; refuse OTA confirmation.\r\n");
        lcd_show_string(30, 110, 220, 16, 16, "CAN Init ERR", RED);
        delay_ms(1000);
        NVIC_SystemReset();
    }

    /*
     * Confirmation is deliberately performed after the core clock, console,
     * display and CAN path are alive, but before an optional DHT11 failure can
     * block startup. A reset before this point leaves the image in TRIAL state,
     * so the bootloader enters recovery instead of repeatedly booting it.
     */
    if (OTA_CONFIRM_DELAY_MS > 0u) {
        printf("OTA confirmation delayed by %lu ms for acceptance test.\r\n",
               (unsigned long)OTA_CONFIRM_DELAY_MS);
        delay_ms(OTA_CONFIRM_DELAY_MS);
    }

    confirm_result = ota_ab_metadata_confirm(
        OTA_AB_APP_SLOT, APP_FIRMWARE_VERSION);
    if (confirm_result == OTA_AB_META_OK) {
        printf("OTA Slot A image %u.%u.%u.%u confirmed.\r\n",
               CAN_OTA_VERSION_MAJOR(APP_FIRMWARE_VERSION),
               CAN_OTA_VERSION_MINOR(APP_FIRMWARE_VERSION),
               CAN_OTA_VERSION_PATCH(APP_FIRMWARE_VERSION),
               CAN_OTA_VERSION_BUILD(APP_FIRMWARE_VERSION));
    } else {
        printf("A/B OTA confirmation failed: result=%u\r\n", confirm_result);
        lcd_show_string(30, 110, 220, 16, 16, "OTA Confirm ERR", RED);
        delay_ms(1000);
        NVIC_SystemReset();
    }
    send_heartbeat(counter);

    if (dht11_init() != 0u) {
        dht_ready = 0;
        dht_ok = 0;
        lcd_show_string(30, 110, 220, 16, 16, "DHT11 Error", RED);
        send_dht11_data(0, 0, dht_ok, counter++);
    } else {
        dht_ready = 1;
        dht_ok = 0;
        lcd_show_string(30, 110, 220, 16, 16, "DHT11 Wait ", RED);
    }

    while (1) {
        handle_control();

        if ((tick_10ms % DHT_SAMPLE_INTERVAL_TICKS) == 0u) {
            /*
             * Sensor recovery is periodic rather than a blocking startup
             * loop. CAN control and the OTA entry command therefore remain
             * responsive even when DHT11 is absent or faulty.
             */
            if (!dht_ready) {
                if (dht11_init() == 0u) {
                    dht_ready = 1;
                    dht_ok = 0;
                    temperature = 0u;
                    humidity = 0u;
                    lcd_show_string(30, 110, 220, 16, 16,
                                    "DHT11 Wait ", RED);
                } else {
                    dht_ready = 0;
                    dht_ok = 0;
                    temperature = 0u;
                    humidity = 0u;
                }
            } else if (read_valid_dht11_sample(&temperature,
                                               &humidity) != 0u) {
                dht_ready = 0;
                dht_ok = 0;
                temperature = 0u;
                humidity = 0u;
                lcd_show_string(30, 110, 220, 16, 16,
                                "DHT11 Error", RED);
            } else {
                dht_ok = 1;
                lcd_show_string(30, 110, 220, 16, 16,
                                "DHT11 OK   ", RED);
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
