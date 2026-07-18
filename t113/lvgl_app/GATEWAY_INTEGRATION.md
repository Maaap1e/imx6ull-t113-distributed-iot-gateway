# T113 Gateway Status Integration

This `app` directory is the authoritative T113 LVGL application.

## Runtime data path

```text
i.MX6ULL gateway
    -> TCP framed data
    -> t113_display_app
    -> /tmp/t113_sensor_state.json
    -> gateway_state.c (500 ms polling)
    -> LVGL pages
```

The state bridge starts in `main.c`. The default file can be overridden for a
simulator or test run:

```sh
export T113_SENSOR_STATE_FILE=/tmp/test_sensor_state.json
```

## Main page

The lower-right corner only shows the two distributed system nodes:

```text
i.MX6ULL   ONLINE / OFFLINE
STM32 CAN  ONLINE / OFFLINE / UNKNOWN
```

`UNKNOWN` means the i.MX6ULL TCP connection is unavailable, so T113 cannot
reliably determine the STM32 CAN node state.

AP3216C, ICM20608 and DHT11 values remain available through
`gateway_state_get()` for dedicated pages. They are intentionally not shown on
the main page.

## OTA ownership

STM32 CAN OTA belongs to the i.MX6ULL local maintenance UI. It should read a
separate `/tmp/stm32_ota_state.json` file produced by the OTA host. OTA controls
and progress are intentionally not included in this T113 application.

## Board verification

Because `gateway_state.c` is a new source file, rerun the CMake configure step
before building so `aux_source_directory(ui/data SOURCES)` discovers it.

Start the T113 TCP receiver before the LVGL app, then verify the state file:

```sh
cat /tmp/t113_sensor_state.json
```

Expected main-page behavior:

```text
No TCP data             i.MX6ULL OFFLINE, STM32 UNKNOWN
TCP data, STM32 offline i.MX6ULL ONLINE,  STM32 OFFLINE
TCP and CAN data        i.MX6ULL ONLINE,  STM32 ONLINE
State file older than 3s i.MX6ULL OFFLINE, STM32 UNKNOWN
```
