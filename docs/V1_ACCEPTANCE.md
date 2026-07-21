# v1.0 acceptance checklist

`v1.0.0-rc.1` means the code and operational tooling are ready for board-level
acceptance. Create the final `v1.0.0` tag only after all required checks pass on
the actual i.MX6ULL, T113 and STM32F103 hardware.

## Required functional checks

- Cold boot both Linux boards and confirm services start without a shell login.
- Confirm AP3216C, ICM20608 and STM32 DHT11 values reach the T113 LVGL UI.
- Disconnect/reconnect Ethernet and confirm TCP reconnects automatically.
- Disconnect/reconnect CAN and confirm offline/online state transitions.
- Kill each Linux application and confirm systemd or the BusyBox supervisor
  restarts it within 10 seconds.
- Perform one successful CAN OTA with the intended release firmware and record
  the final CRC32 and version.
- Power-cycle after OTA and confirm the STM32 application boots normally.

## Protocol and resource checks

```sh
# Native development host
make -C tests test
python3 tests/tcp_fault_injector.py <t113-ip> --mode fragmented
python3 tests/tcp_fault_injector.py <t113-ip> --mode coalesced
python3 tests/tcp_fault_injector.py <t113-ip> --mode bad-crc
python3 tests/tcp_fault_injector.py <t113-ip> --mode oversize

# Each Linux board
/opt/iot-gateway/scripts/healthcheck.sh imx6ull   # or: t113
```

Run the complete system for at least 24 hours. At the beginning and end record:

```sh
date
uptime
free -h 2>/dev/null || free
df -h /tmp
du -h /tmp/*.log /tmp/*.csv 2>/dev/null
```

Acceptance requires no unexplained process exit, steadily growing available-RAM
loss, unbounded runtime file, UI lock-up, or unrecovered CAN/TCP disconnection.

## Release evidence

Attach a short acceptance record to the GitHub Release: board images/versions,
commit ID, toolchains, CAN bitrate, TCP port, 24-hour start/end resource values,
OTA firmware CRC32, and any known limitations. The current OTA verifies integrity
with CRC32; it does not yet provide cryptographic authenticity, rollback, or
power-loss resume, so do not describe those as v1.0 capabilities.
