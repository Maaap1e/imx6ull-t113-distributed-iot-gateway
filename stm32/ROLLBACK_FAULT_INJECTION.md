# A/B rollback fault-injection firmware

The `dht11_can_app_slot_a_rollback_test` and
`dht11_can_app_slot_b_rollback_test` projects are acceptance-test fixtures,
not production firmware.

- Both images report version `2.1.0.2`.
- Slot A behaves normally and is included only so the signed OTA3 package is
  structurally complete.
- Slot B delays `ota_ab_metadata_confirm()` for 30 seconds. During this window,
  reset or power-cycle the STM32 to simulate a trial image that resets before
  confirmation.
- With confirmed Slot A version `2.1.0.1`, the next boot must discard the Slot
  B candidate and roll back to Slot A.

Expected Host outcome: the OTA transfer reaches `done`, but the Host does not
receive a matching confirmed `2.1.0.2` heartbeat and therefore times out.
Expected device outcome: the Bootloader reports rollback and the normal sensor
client returns to version `2.1.0.1` from Slot A.
