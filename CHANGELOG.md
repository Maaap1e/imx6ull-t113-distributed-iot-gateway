# Changelog

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

### Release status

This is a release candidate. Promote it to `v1.0.0` only after the cold-boot,
fault-recovery, CAN OTA, and 24-hour real-board checks in
`docs/V1_ACCEPTANCE.md` pass.
