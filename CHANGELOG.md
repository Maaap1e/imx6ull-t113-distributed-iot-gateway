# Changelog

## 1.0.0 - 2026-07-23

### Added

- Added real-hardware integration and LVGL UI showcase images.

### Validated

- Completed a 24-hour real-board stability test.
- Verified Ethernet and CAN disconnect/recovery behavior.
- Verified automatic recovery of the i.MX6ULL gateway and T113 receiver.
- Completed STM32 CAN OTA from application 1.0 to 1.1.
- Verified STM32 application 1.1 after a full power cycle.
- Verified LVGL UI operation and added complete board-level acceptance evidence.

### Known limitations

- CAN OTA uses CRC32 integrity checking without firmware signatures or rollback.
- Non-login SSH OTA execution requires `/sbin` in `PATH`.
- The i.MX6ULL payload timestamp has a documented UTC/CST display difference.

## 1.0.0-rc.1 - 2026-07-21

### Added

- Shared runtime configuration for i.MX6ULL and T113.
- BusyBox init scripts, systemd units, process supervision, and health checks.
- Fixed-size log/CSV rotation suitable for the boards' `/tmp` tmpfs mounts.
- Target installation and unified cross-build scripts.
- Ubuntu-VM release packaging into separate i.MX6ULL and T113 archives.
- Native TCP protocol tests, network fault injection, and GitHub Actions CI.
- Board-level `v1.0.0` acceptance checklist.

### Changed

- The i.MX6ULL gateway now exits cleanly on `SIGINT` and `SIGTERM`.
- Start/stop scripts use configured paths and preserve runtime history.
- CAN OTA coordinates with both BusyBox and systemd supervision so the CAN
  telemetry client does not compete for OTA status frames.
- BusyBox installation registers the service in `rc.local` on vendor images
  that do not automatically scan newly added `S90*` init scripts.
- T113 installation now uses a native OpenWrt/Tina `rc.common` service and
  enables its `/etc/rc.d` boot link and vendor `load_script.conf` entry.
- T113 receive timeouts are classified as disconnects instead of malformed TCP
  protocol frames, avoiding false protocol-error alarms during peer reboot.

### Release status

This is a release candidate. Promote it to `v1.0.0` only after the cold-boot,
fault-recovery, CAN OTA, and 24-hour real-board checks in
`docs/V1_ACCEPTANCE.md` pass.
