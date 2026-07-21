# Build and Deployment Notes

## Ubuntu VM build host

Linux applications are cross-compiled in the Ubuntu virtual machine. Keep the
repository in the VM's Linux filesystem when possible. Set each compiler to the
real absolute path supplied by the matching board SDK/toolchain; the names below
are examples, not universal toolchain names.

```sh
export IMX_CC=/opt/imx6ull-toolchain/bin/arm-linux-gnueabihf-gcc
export T113_CC=/opt/t113-toolchain/bin/arm-openwrt-linux-gcc

IMX_CC="$IMX_CC" sh scripts/build_all.sh imx6ull
T113_CC="$T113_CC" sh scripts/build_all.sh t113

file linux/imx6ull_gateway/imx6ull_gateway_app
file t113/tcp_receiver/t113_display_app
```

The `file` output must report an ARM executable, not x86-64. Native GCC is used
only for host protocol tests (`make -C tests test`).

`build_all.sh` covers the standalone Linux user-space programs only. Kernel
drivers/device trees, STM32 Keil projects, and the complete T113 LVGL target must
still be built with their corresponding BSP/SDK workflows.

## Create and transfer target bundles

```sh
sh scripts/package_target.sh imx6ull
sh scripts/package_target.sh t113

scp dist/iot-gateway-1.0.0-rc.1-imx6ull.tar.gz root@<IMX6ULL_IP>:/tmp/
scp dist/iot-gateway-1.0.0-rc.1-t113.tar.gz root@<T113_IP>:/tmp/
```

The packaging script also writes a `.sha256` file when `sha256sum` is available.
Copy it with the archive and verify it on the board before extraction.

On the matching board, extract its bundle and run `sh scripts/install_target.sh
<role>`. The installer preserves an existing `/etc/iot-gateway/<role>.conf` and
writes new defaults as `<role>.conf.new`. See `docs/RUNTIME_MANAGEMENT.md` for
BusyBox, systemd, health checks, and bounded tmpfs logs.

## STM32 projects

Open the `.uvprojx` file under each `Projects/MDK-ARM` directory with Keil MDK.
The bootloader and application use different link addresses:

| Image | Flash address |
|---|---:|
| CAN OTA bootloader | `0x08000000` |
| DHT11 CAN application | `0x08010000` |

Build output is intentionally ignored. Keep `.bin`, `.hex`, `.axf`, `.map` and
object files out of source commits. Publish tested firmware as versioned GitHub
Release attachments instead.

## T113 LVGL application

The public directory contains the application layer, not the complete T113 SDK.
Integrate `t113/lvgl_app` into the corresponding SDK application directory so
its CMake target can use the platform port, LVGL, Wi-Fi, HTTP, player and font
components supplied by that SDK.

The UI expects the T113 TCP receiver to maintain:

```text
/tmp/t113_sensor_state.json
```

Override the path during simulation with:

```sh
export T113_SENSOR_STATE_FILE=/tmp/test_sensor_state.json
```

The weather API key is read from `WEATHER_API_KEY`; it is never stored in the
public source tree.

## Recommended boot order

1. Bring up CAN on i.MX6ULL.
2. Start `stm32_can_sensor_client`.
3. Start `t113_display_app` on T113.
4. Start the i.MX6ULL TCP gateway.
5. Start the T113 LVGL application.

Start scripts use background processes, PID files and bounded logs so the serial
console remains usable. Production startup should use the supplied BusyBox init
script or systemd units instead of shell-login startup commands.
