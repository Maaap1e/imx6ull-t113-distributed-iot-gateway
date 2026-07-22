# Runtime management

This document covers the `v1.0.0-rc.1` Linux runtime layer. The target layout is
`/opt/iot-gateway`, with editable configuration in `/etc/iot-gateway`.

## Build, package, and install

Cross-compile and package in the Ubuntu VM:

```sh
IMX_CC=/absolute/path/to/arm-linux-gnueabihf-gcc sh scripts/build_all.sh imx6ull
T113_CC=/absolute/path/to/t113-gcc sh scripts/build_all.sh t113
sh scripts/package_target.sh imx6ull
sh scripts/package_target.sh t113
```

Copy the matching archive from `dist/` to `/tmp` on each board, extract it, and
run the installer from the extracted directory as root:

```sh
sh scripts/install_target.sh imx6ull  # on i.MX6ULL
sh scripts/install_target.sh t113     # on T113
```

The board does not compile source code. It only installs the already cross-built
executables and runtime files. `DESTDIR=/some/staging/root` remains available for
distribution/image builders.

Edit `/etc/iot-gateway/imx6ull.conf` and set at least `T113_IP`, `CAN_IFACE`,
and the actual sensor device paths. Edit `/etc/iot-gateway/t113.conf` if the
listen address or port differs.

## BusyBox / SysV init

The installer copies `S90iot-imx6ull` or `S90iot-t113` when `/etc/init.d` exists.
Some vendor images do not scan arbitrary `S90*` files; when `/etc/rc.local`
exists, the installer also inserts the matching init-script command before
`exit 0`. The script starts a lightweight supervisor that restarts a missing
application and rotates runtime files. Manual commands are:

```sh
/etc/init.d/S90iot-imx6ull start
/opt/iot-gateway/scripts/healthcheck.sh imx6ull

/etc/init.d/S90iot-t113 start
/opt/iot-gateway/scripts/healthcheck.sh t113
```

After installation, confirm the boot hook with `grep S90iot /etc/rc.local`. If
the firmware uses neither `rc.local` nor an `S*` scan, add the init script to its
documented boot hook. Keep a recoverable copy before editing another vendor
startup file.

## OpenWrt / Tina rc.common

Tina images that provide `/etc/rc.common` and `/etc/rc.d` use the native
`/etc/init.d/iot-t113` wrapper. The installer calls `enable`, which creates the
boot link automatically. Vendor Tina images whose `rcS` reads
`/etc/init.d/load_script.conf` are also registered in that file. Verify and
control it with:

```sh
ls -l /etc/rc.d/*iot-t113*
grep '^iot-t113$' /etc/init.d/load_script.conf 2>/dev/null
/etc/init.d/iot-t113 start
/etc/init.d/iot-t113 stop
```

## systemd

On i.MX6ULL:

```sh
systemctl daemon-reload
systemctl enable --now iot-can-sensor.service iot-imx6ull-gateway.service
systemctl enable --now iot-runtime-maintenance@imx6ull.service
```

On T113:

```sh
systemctl daemon-reload
systemctl enable --now iot-t113-receiver.service
systemctl enable --now iot-runtime-maintenance@t113.service
```

systemd directly supervises foreground processes. Do not also start the BusyBox
supervisor on a systemd installation.

## CAN OTA and process supervision

Always launch OTA through `/opt/iot-gateway/linux/can_ota_host/run_ota.sh`. It
creates the configured `OTA_LOCK`, pauses the competing CAN sensor client, and
restores that client on every exit path. The BusyBox supervisor honors this lock;
on systemd, the wrapper temporarily stops and restarts `iot-can-sensor.service`.
Launching the raw OTA binary bypasses this coordination and is not recommended.

## Bounded files in tmpfs

The confirmed boards mount `/tmp` on tmpfs: approximately 248 MiB on i.MX6ULL
and 54 MiB on T113. Defaults therefore cap logs near 1 MiB and CSV data at
4--5 MiB, retaining only a small fixed number of archives. Tune
`LOG_MAX_BYTES`, `LOG_KEEP`, `CSV_MAX_BYTES`, and `CSV_KEEP` in the config.

Logs use copy-and-truncate because processes keep the log descriptor open. CSV
files use rename rotation because writers reopen the CSV on each sample. JSON
state files are overwritten atomically and do not grow indefinitely.

These limits protect RAM, not persistent history: `/tmp` is lost after reboot.
For long-term data, export selected records to a server or a dedicated data
partition with its own retention policy; avoid unlimited writes to the SD card.
