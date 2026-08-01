#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/KEY/key.h"
#include "./IAP/iap.h"
#include "./CAN_OTA/can_ota.h"

#define OTA_ENTER_WAIT_MS 3000u

static void boot_ui_show(void)
{
    lcd_show_string(30, 50, 220, 16, 16, "STM32F103", RED);
    lcd_show_string(30, 70, 220, 16, 16, "A/B Secure CAN OTA", RED);
    lcd_show_string(30, 90, 220, 16, 16, "SHA256 + ECDSA P256", RED);
    lcd_show_string(30, 110, 220, 16, 16, "Waiting CAN OTA...", BLUE);
}

static uint8_t slot_is_bootable(const ota_ab_metadata_t *metadata,
                                uint8_t slot)
{
    if (!ota_ab_slot_id_is_valid(slot)) {
        return 0u;
    }
    return can_ota_ab_image_matches_slot(slot, &metadata->slots[slot]);
}

static void jump_to_slot(uint8_t slot)
{
    uint32_t address = ota_ab_slot_address(slot);

    printf("Verified Slot %c, jump to 0x%08lX\r\n",
           slot == CAN_OTA_AB_SLOT_A ? 'A' : 'B',
           (unsigned long)address);
    lcd_show_string(30, 130, 220, 16, 16,
                    slot == CAN_OTA_AB_SLOT_A ?
                    "Jump Slot A..." : "Jump Slot B...", BLUE);
    delay_ms(100u);
    iap_load_app(address);
}

static void rollback_and_jump(ota_ab_metadata_t *metadata,
                              const char *reason)
{
    uint8_t confirmed = metadata->confirmed_slot;

    printf("Rollback: %s\r\n", reason);
    if (!ota_ab_slot_id_is_valid(confirmed) ||
        !slot_is_bootable(metadata, confirmed)) {
        printf("No verified confirmed slot; recovery mode required.\r\n");
        return;
    }
    if (ota_ab_metadata_rollback() != OTA_AB_META_OK) {
        printf("Failed to persist rollback decision.\r\n");
        return;
    }
    can_ota_ab_send_status(CAN_OTA_AB_STATUS_ROLLBACK,
                           CAN_OTA_AB_ERR_NONE, 0u, 0u);
    jump_to_slot(confirmed);
}

static void jump_to_app_if_valid(void)
{
    ota_ab_metadata_t metadata;
    ota_ab_meta_result_t result = ota_ab_metadata_load(&metadata);
    uint8_t slot;

    if (result == OTA_AB_META_EMPTY) {
        result = ota_ab_metadata_import_legacy();
        if (result == OTA_AB_META_OK) {
            printf("Imported v1 confirmed image as legacy Slot A.\r\n");
            result = ota_ab_metadata_load(&metadata);
        }
    }
    if (result != OTA_AB_META_OK) {
        printf("No valid A/B metadata; recovery required (%u).\r\n", result);
        return;
    }

    printf("A/B state: active=%u confirmed=%u candidate=%u attempted=%u\r\n",
           metadata.active_slot, metadata.confirmed_slot,
           metadata.candidate_slot, metadata.trial_booted);

    if (metadata.candidate_slot != CAN_OTA_AB_SLOT_NONE) {
        slot = metadata.candidate_slot;
        if (metadata.trial_booted != 0u) {
            rollback_and_jump(
                &metadata, "trial image reset before confirmation");
            return;
        }
        if (!slot_is_bootable(&metadata, slot)) {
            rollback_and_jump(
                &metadata, "candidate hash, CRC or vector invalid");
            return;
        }
        /*
         * Persist the one allowed boot attempt before jumping. A power loss
         * before App confirmation causes deterministic rollback next boot.
         */
        if (ota_ab_metadata_mark_trial(slot) != OTA_AB_META_OK) {
            printf("Failed to persist trial attempt.\r\n");
            return;
        }
        printf("Starting one-shot trial for Slot %c.\r\n",
               slot == CAN_OTA_AB_SLOT_A ? 'A' : 'B');
        jump_to_slot(slot);
        return;
    }

    slot = metadata.active_slot;
    if (!slot_is_bootable(&metadata, slot)) {
        printf("Active image failed integrity check.\r\n");
        return;
    }
    jump_to_slot(slot);
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
    can_ota_ab_init();

    boot_ui_show();
    printf("\r\nSTM32F103 A/B Secure CAN OTA Bootloader\r\n");
    printf("Slot A: 0x%08lX, Slot B: 0x%08lX, size: %lu bytes\r\n",
           (unsigned long)CAN_OTA_AB_SLOT_A_ADDRESS,
           (unsigned long)CAN_OTA_AB_SLOT_B_ADDRESS,
           (unsigned long)CAN_OTA_AB_SLOT_SIZE);
    printf("Next update target: Slot %c\r\n",
           can_ota_ab_target_slot() == CAN_OTA_AB_SLOT_A ? 'A' : 'B');

    ota_started = can_ota_ab_wait_enter(OTA_ENTER_WAIT_MS);
    if (!ota_started) {
        jump_to_app_if_valid();
        printf("No bootable App; enter signed OTA recovery mode.\r\n");
        lcd_show_string(30, 130, 220, 16, 16, "OTA Recovery", BLUE);
        while (!can_ota_ab_wait_enter(1000u)) {
            LED0_TOGGLE();
        }
    }

    while (1) {
        printf("Signed A/B OTA started; target Slot %c.\r\n",
               can_ota_ab_target_slot() == CAN_OTA_AB_SLOT_A ? 'A' : 'B');
        lcd_show_string(30, 130, 220, 16, 16, "OTA Running...", BLUE);

        if (can_ota_ab_run() == 0u) {
            printf("OTA verified, start one-shot trial App.\r\n");
            lcd_show_string(30, 150, 220, 16, 16, "OTA Verified!", BLUE);
            delay_ms(300u);
            jump_to_app_if_valid();
        } else {
            printf("OTA rejected or failed; confirmed slot preserved.\r\n");
            lcd_show_string(30, 150, 220, 16, 16, "OTA Failed!", BLUE);
        }

        printf("Waiting for another signed OTA request.\r\n");
        while (!can_ota_ab_wait_enter(1000u)) {
            LED0_TOGGLE();
        }
    }
}
