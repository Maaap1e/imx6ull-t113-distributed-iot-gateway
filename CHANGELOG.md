# Changelog

## 1.1.0 - 2026-07-26

### Released

- Promoted `v1.1.0-rc.1` to the stable `v1.1.0` release.
- No runtime code changes were introduced after the release candidate.
- The core three-node gateway completed a 24-hour hardware acceptance test.
- The MQTT and Qt increment completed a 3-hour 8-minute continuous-operation test.
- Verified MQTT telemetry, CSV recording, LED closed-loop control, process recovery and network recovery.

## 1.1.0-rc.1 - 2026-07-26

### Added

- Added an optional i.MX6ULL MQTT northbound bridge based on Eclipse Paho.
- Added atomic `/tmp/imx6ull_gateway_state.json` output so TCP display and
  MQTT telemetry remain decoupled.
- Added MQTT LWT status, bounded reconnect backoff, telemetry publication,
  allow-listed LED commands, execution responses, supervision and health
  checks.
- Added MQTT command payload tests, including clients that append NUL or CRLF.
- Added a Windows Qt Widgets MQTT dashboard with overview, trend, control and
  settings pages.
- Added strict telemetry JSON validation, Chinese/English UI translation,
  daily UTF-8 CSV recording and Qt parser tests.
- Added `v1.1.0-rc.1` MQTT/Qt real-hardware acceptance evidence.

### Validated

- Completed a 3-hour 8-minute MQTT and Qt continuous-operation test.
- Received more than 2,300 telemetry messages with zero Qt parse errors.
- Verified closed-loop LED off, on and heartbeat commands with device
  execution responses.
- Verified healthy supervisor, CAN client, TCP gateway and MQTT bridge
  processes at the end of the test.
- Verified CAN ERROR-ACTIVE state with zero receive errors, drops, overruns and
  bus-off events at the end of the test.

### Security

- MQTT is disabled by default. The bundled Tongxinmao Broker settings
  are for public smoke tests only; production deployments should use a trusted
  Broker with authentication and transport security.

### Known limitations

- The public MQTT smoke-test path does not provide production-grade TLS,
  authentication or topic isolation.
- Qt trend history is held in process memory; raw telemetry is persisted to
  CSV.
- Device epoch/timezone differences remain visible in raw data; charts use PC
  receive time.

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
