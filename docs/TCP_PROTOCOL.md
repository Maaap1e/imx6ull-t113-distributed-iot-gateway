# TCP Protocol

The i.MX6ULL gateway is a TCP client. The T113 receiver is a TCP server. The
default endpoint is port `5000` and can be overridden by command-line options or
the start scripts.

## Frame format

Every frame starts with a fixed 20-byte network-byte-order header.

| Offset | Size | Field | Description |
|---:|---:|---|---|
| 0 | 2 | magic | `0x55AA` |
| 2 | 1 | version | Currently `1` |
| 3 | 1 | type | `1` data, `2` heartbeat |
| 4 | 4 | sequence | Monotonically increasing frame sequence |
| 8 | 4 | timestamp | Unix timestamp in seconds |
| 12 | 4 | payload length | Maximum `4096` bytes |
| 16 | 4 | CRC32 | CRC32 of the payload only |

The CRC uses reflected polynomial `0xEDB88320`, initial value `0xFFFFFFFF` and
final bitwise inversion.

## Stream handling

TCP is a byte stream and does not preserve application message boundaries. Both
sender and receiver therefore use full-write/full-read loops. The receiver first
collects exactly 20 header bytes, validates magic/version/length, then reads the
declared payload length and verifies CRC32. This handles both split and
coalesced TCP packets.

A socket receive timeout is treated as an idle/disconnected peer and increments
the disconnect counter. It is not counted as a malformed-frame protocol error.

## Data payload

Data frames carry UTF-8 JSON. A representative payload is:

```json
{
  "node": "imx6ull",
  "ap3216c_ok": 1,
  "ap3216c": {"als": 120, "ir": 3, "ps": 9},
  "icm20608_ok": 1,
  "icm20608": {
    "gyro_x": -1.28,
    "gyro_y": -0.24,
    "gyro_z": -0.49,
    "accel_x": 0.01,
    "accel_y": 0.03,
    "accel_z": 0.99,
    "temp": 37.06
  },
  "stm32_can_ok": 1,
  "stm32_can": {
    "node": "stm32f103",
    "online": 1,
    "dht_ok": 1,
    "temperature": 25,
    "humidity": 60,
    "app_version": "1.0",
    "counter": 72
  }
}
```

The exact payload also contains gateway runtime counters. Consumers should
ignore unknown JSON fields so the protocol can be extended without breaking
older UI versions.

## Runtime state

After validation, the T113 process writes the latest state through a temporary
file followed by `rename()`. This avoids exposing partially written JSON to the
LVGL polling thread. A state older than the UI timeout is treated as offline.
