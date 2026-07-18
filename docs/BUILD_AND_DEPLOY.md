# Build and Deployment Notes

## Linux applications

Each Linux module has a small Makefile. Native GCC can be used for protocol
testing; board deployment requires the matching cross compiler.

```sh
make -C linux/imx6ull_gateway CC=arm-linux-gnueabihf-gcc
make -C linux/can_sensor_client CC=arm-linux-gnueabihf-gcc
make -C linux/can_ota_host CC=arm-linux-gnueabihf-gcc
make -C t113/tcp_receiver CC=arm-openwrt-linux-gcc
```

Copy only the resulting executable and start/stop scripts to the target. Runtime
logs, PID files, CSV files and JSON state stay under `/tmp` and must not be
committed.

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

Start scripts use background processes, PID files and logs so the serial console
remains usable.
