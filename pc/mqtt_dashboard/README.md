# MQTT Dashboard for Windows

Qt Widgets dashboard for the northbound MQTT interface of the distributed
i.MX6ULL/T113/STM32 gateway.

## Features

- Overview cards for MQTT, i.MX6ULL telemetry freshness, T113 TCP and STM32 CAN
- AP3216C, ICM20608 and STM32/DHT11 live values
- Qt Charts trend view with 1/10/30/60/180 minute windows
- Closed-loop LED control (`0`, `1`, `2`) with response timeout handling
- Daily UTF-8 CSV recording
- Runtime Chinese/English UI translation
- `QSettings` persistence without storing the MQTT password
- Strict JSON validation and parser tests
- Built-in demo source for UI development without a Broker
- Optional Eclipse Paho MQTT C backend

The PC receive time is used for chart timestamps. The raw i.MX6ULL epoch is
kept separately in CSV, so a device timezone/RTC mismatch is never hidden by a
hard-coded eight-hour correction.

## Topics

The default prefix is `/public/TEST/Maaap1e/imx6ull-01`.

| Topic | Direction | Purpose |
|---|---|---|
| `<prefix>/telemetry` | subscribe | Aggregate gateway JSON |
| `<prefix>/status` | subscribe | Retained online/offline state |
| `<prefix>/cmd/response` | subscribe | LED execution result |
| `<prefix>/cmd/led` | publish | ASCII `0`, `1`, or `2` |

The public Broker is for smoke tests only. Do not put credentials, OTA, shell
commands or safety-critical controls in a public namespace.

## Build the verified demo configuration

Qt 5.12.9 MinGW 7.3 64-bit and Qt Charts are required.

In Qt Creator, open `mqtt_dashboard.pro` and select the **Desktop Qt 5.12.9
MinGW 64-bit** kit. Do not select Qt Creator's bundled `clang.exe`; it is a
code-model tool in this installation, not the compiler paired with the Qt
5.12.9 MinGW libraries.

```powershell
$QtRoot = "D:\Qt\Qt5.12.9"
$Qmake = "$QtRoot\5.12.9\mingw73_64\bin\qmake.exe"
$Make = "$QtRoot\Tools\mingw730_64\bin\mingw32-make.exe"

New-Item -ItemType Directory -Force build-mqtt-dashboard | Out-Null
Set-Location build-mqtt-dashboard
& $Qmake ..\pc\mqtt_dashboard\mqtt_dashboard.pro
& $Make -j4
```

Start the application, keep **Use built-in demo data** enabled, and press
Connect. This validates Widgets, Charts, JSON parsing, trends, commands,
translations and CSV without a Paho dependency.

Run parser tests from a separate build directory:

```powershell
New-Item -ItemType Directory -Force build-mqtt-dashboard-tests | Out-Null
Set-Location build-mqtt-dashboard-tests
& $Qmake ..\pc\mqtt_dashboard\tests\parser_test.pro
& $Make -j4
.\release\mqtt_dashboard_parser_test.exe
```

## Build with Eclipse Paho MQTT C

Install or build a **64-bit Windows MinGW-compatible** Paho MQTT C package. Its
prefix must contain:

```text
include/MQTTClient.h
lib/libpaho-mqtt3c.dll.a
bin/libpaho-mqtt3c.dll
```

Then configure qmake with:

```powershell
& $Qmake `
  "CONFIG+=paho" `
  "PAHO_ROOT=D:/path/to/paho-mingw64" `
  ..\pc\mqtt_dashboard\mqtt_dashboard.pro
& $Make -j4
```

Copy the Paho DLL beside `mqtt_dashboard.exe`, disable Demo mode in Settings,
and connect with a unique PC Client ID.

This project was verified with Eclipse Paho MQTT C 1.3.16 built by the same
MinGW 7.3 64-bit toolchain as Qt. When building Paho with this older MinGW,
define `_WIN32_WINNT=0x0600` so the required Windows socket APIs are exposed:

```powershell
cmake -S paho.mqtt.c -B paho-build -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_INSTALL_PREFIX=D:/path/to/paho-mingw64 `
  -DPAHO_BUILD_STATIC=OFF `
  -DPAHO_WITH_SSL=OFF `
  -DPAHO_BUILD_SAMPLES=OFF `
  -DPAHO_BUILD_DOCUMENTATION=OFF `
  -DCMAKE_C_FLAGS=-D_WIN32_WINNT=0x0600
cmake --build paho-build --parallel
cmake --install paho-build
```

In Qt Creator, the equivalent qmake arguments are:

```text
CONFIG+=paho PAHO_ROOT=D:/path/to/paho-mingw64
```

The CMake build exposes the equivalent options:

```text
-DMQTT_DASHBOARD_WITH_PAHO=ON
-DPAHO_ROOT=D:/path/to/paho-mingw64
```

## Deploy

After a release build:

```powershell
$Deploy = "dist\mqtt-dashboard"
New-Item -ItemType Directory -Force $Deploy | Out-Null
Copy-Item .\release\mqtt_dashboard.exe $Deploy
& "$QtRoot\5.12.9\mingw73_64\bin\windeployqt.exe" `
  --release --compiler-runtime "$Deploy\mqtt_dashboard.exe"
```

For a Paho build, also copy `paho-mqtt3c.dll` and retain the Eclipse Paho
license notices.

## Architecture

UI pages never parse raw MQTT messages:

```text
MqttTransport -> TelemetryParser -> TelemetrySample
                                      |-> OverviewPage
                                      |-> TrendsPage
                                      `-> CsvRecorder

ControlPage -> publish cmd/led -> cmd/response -> result/timeout
```

The Paho backend performs connect/publish operations on a worker thread and
uses bounded reconnect backoff. The demo backend implements the same
`MqttTransport` interface.

For repeatable UI smoke tests, the executable accepts `--smoke-test`,
`--live-test`, `--page=0..3` and `--screenshot=<file.png>`. Smoke mode forces
the local demo source; live-test mode forces the Paho backend. Both disable CSV
without changing saved user settings.
