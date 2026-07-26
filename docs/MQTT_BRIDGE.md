# MQTT Northbound Bridge

The optional i.MX6ULL MQTT bridge exports the gateway's existing aggregate JSON
state to a PC or server without coupling Broker availability to CAN collection
or the T113 TCP display path.

```text
STM32/CAN + AP3216C + ICM20608
                 |
          imx6ull_gateway
            |          |
       framed TCP   atomic JSON
            |          |
         T113      mqtt_bridge
                       |
                  MQTT Broker
                       |
                      PC
```

MQTT is disabled by default. Public Brokers are suitable only for smoke tests;
do not expose OTA, shell commands, secrets, or safety-critical control through
the public test namespace.

## Topics

With `MQTT_TOPIC_PREFIX=/public/TEST/Maaap1e/imx6ull-01`:

| Topic suffix | Direction | QoS | Retain | Payload |
|---|---|---:|---:|---|
| `telemetry` | device to Broker | 0 | no | Aggregate gateway JSON |
| `status` | device to Broker | 1 | yes | Online/offline JSON and LWT |
| `cmd/led` | Broker to device | 1 | no | ASCII `0`, `1`, or `2` |
| `cmd/response` | device to Broker | 1 | no | Command result JSON |

LED values are `0` for off, `1` for on, and `2` for the kernel heartbeat
trigger. The parser safely accepts a trailing NUL, CRLF, or whitespace because
some desktop test clients include a terminator in their MQTT payload.

QoS 1 can redeliver a command. LED state assignment is idempotent; future
non-idempotent commands must carry a `request_id` and use a deduplication cache.

## Cross-build

Build Eclipse Paho MQTT C for the i.MX6ULL hard-float ABI first. Assuming the
result is under `~/tools/paho.mqtt.c/install-arm`:

```sh
export IMX_CC=arm-linux-gnueabihf-gcc
export PAHO_PREFIX="$HOME/tools/paho.mqtt.c/install-arm"

make -C linux/mqtt_bridge clean
make -C linux/mqtt_bridge \
  CC="$IMX_CC" \
  PAHO_PREFIX="$PAHO_PREFIX"

file linux/mqtt_bridge/mqtt_bridge
readelf -l linux/mqtt_bridge/mqtt_bridge | grep interpreter
readelf -A linux/mqtt_bridge/mqtt_bridge | grep Tag_ABI_VFP_args
```

The interpreter must be `/lib/ld-linux-armhf.so.3`, and the ABI output must
report VFP register arguments.

The unified build command also accepts the prefix:

```sh
IMX_CC="$IMX_CC" PAHO_PREFIX="$PAHO_PREFIX" \
  sh scripts/build_all.sh imx6ull
```

## Package and install

Pass the directory that contains the ARM Paho shared-library symlinks:

```sh
PAHO_PREFIX="$PAHO_PREFIX" \
PAHO_LIB_DIR="$PAHO_PREFIX/lib" \
PACKAGE_VERSION=1.1.0-rc.1 \
  sh scripts/package_target.sh imx6ull
```

The package preserves `libpaho-mqtt3c.so*` symlinks and installs them under
`/opt/iot-gateway/lib`. The executable contains an rpath for that directory;
the runtime wrappers also set `LD_LIBRARY_PATH` explicitly.
`PACKAGE_VERSION` labels the MQTT validation bundle without changing the
repository's released `VERSION`; update `VERSION` only after acceptance passes.

After installing the package, merge new settings from
`/etc/iot-gateway/imx6ull.conf.new` if an older configuration was preserved.
For the verified public smoke-test Broker:

```sh
MQTT_ENABLE=1
MQTT_BROKER_URI="tcp://mq.tongxinmao.com:18830"
MQTT_CLIENT_ID="Maaap1e-imx6ull-01"
MQTT_TOPIC_PREFIX="/public/TEST/Maaap1e/imx6ull-01"
MQTT_PUBLISH_INTERVAL_SECONDS=5
```

Client IDs must be unique. Use another ID such as `Maaap1e-pc-01` for the PC
client. The public Tongxinmao namespace requires the
`/public/TEST/` prefix.

### PC client smoke test

Configure the desktop client as follows:

| Setting | Value |
|---|---|
| Host | `mq.tongxinmao.com` |
| TCP port | `18830` |
| TLS/SSL | disabled |
| Username/password | empty |
| Client ID | a unique value such as `Maaap1e-pc-01` |
| Subscribe topic | `/public/TEST/Maaap1e/imx6ull-01/#` |

After the board is online, the PC should receive aggregate JSON on
`/public/TEST/Maaap1e/imx6ull-01/telemetry` and retained status on
`/public/TEST/Maaap1e/imx6ull-01/status`.

Publish a plain-text command to
`/public/TEST/Maaap1e/imx6ull-01/cmd/led`:

- `0`: LED off
- `1`: LED on
- `2`: kernel heartbeat trigger

The result is returned on
`/public/TEST/Maaap1e/imx6ull-01/cmd/response`. The formal bridge accepts the
extra NUL or line ending added by some desktop clients, so HEX mode is no
longer required. When diagnosing an unfamiliar client, raw HEX `30`, `31`,
and `32` can still be used to prove whether payload formatting is the cause.

Restart and verify on BusyBox:

```sh
/etc/init.d/S90iot-imx6ull restart
/opt/iot-gateway/scripts/healthcheck.sh imx6ull
cat /tmp/imx6ull_gateway_state.json
tail -n 50 /tmp/mqtt_bridge.log
```

For systemd, enable the bridge only after `MQTT_ENABLE=1` is configured:

```sh
systemctl daemon-reload
systemctl enable --now iot-mqtt-bridge.service
```

## Failure behavior

- Broker/DNS/network failure: the bridge retries with bounded exponential
  backoff from 1 to 30 seconds.
- MQTT process exit: BusyBox supervision or systemd restarts only the bridge.
- T113 TCP outage: the gateway continues refreshing the local JSON state, so
  MQTT telemetry remains available.
- Clean shutdown: the bridge publishes retained offline status.
- Unexpected disconnect: the Broker publishes the retained LWT offline status.
- Invalid command: no shell input is executed; a failure response is published.

## Acceptance checklist

1. Confirm telemetry contains AP3216C, ICM20608, STM32 CAN, and TCP status.
2. Confirm `status` changes between retained online and offline states.
3. Disconnect the Broker/network and verify CAN, TCP, and LVGL remain healthy.
4. Restore the network and measure reconnect time.
5. Kill `mqtt_bridge` and verify the supervisor assigns a new PID.
6. Send ASCII and terminator-appended LED payloads and verify responses.
7. Confirm `/tmp/mqtt_bridge.log` rotates at the configured limit.
8. Repeat a bounded soak test before tagging `v1.1.0`.
