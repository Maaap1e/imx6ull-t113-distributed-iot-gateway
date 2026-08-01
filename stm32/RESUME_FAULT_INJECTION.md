# Resumable OTA power-loss acceptance firmware

The `dht11_can_app_slot_a_resume_test` and
`dht11_can_app_slot_b_resume_test` projects are normal-confirming version
`2.1.0.3` images built separately for the power-loss/resume acceptance test.

With confirmed Slot A version `2.1.0.1`, the Bootloader selects Slot B. Remove
power from the STM32 only after Host progress reaches about 30%. After the Host
times out, restore STM32 power and resend the exact same OTA3 file. A successful
resume reports a non-zero 2KB-aligned offset, completes Slot B, and confirms
version `2.1.0.3`.

Do not change, regenerate, rename through repackaging, or replace the OTA3 file
between the interrupted and resumed sessions: its signed package identity must
remain identical.
