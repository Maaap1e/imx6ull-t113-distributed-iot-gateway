# CAN Telemetry and Control Protocol

The current setup uses standard 11-bit identifiers at `500000` bit/s.

## Identifier allocation

| CAN ID | Direction | Function |
|---:|---|---|
| `0x101` | STM32 -> i.MX6ULL | Application heartbeat |
| `0x102` | STM32 -> i.MX6ULL | DHT11 temperature and humidity |
| `0x201` | i.MX6ULL -> STM32 | LED or bootloader control command |
| `0x202` | STM32 -> i.MX6ULL | Control acknowledgement |

All application frames use 8 data bytes. Byte 7 is the unsigned 8-bit sum of
bytes 0 through 6.

## `0x101` heartbeat

| Byte | Meaning |
|---:|---|
| 0 | Application major version |
| 1 | Application minor version |
| 2 | Counter low byte |
| 3 | Counter high byte |
| 4..6 | Reserved |
| 7 | Checksum |

## `0x102` DHT11 sample

| Byte | Meaning |
|---:|---|
| 0 | Integer temperature in degrees Celsius |
| 1 | Temperature decimal, currently reserved as zero |
| 2 | Integer relative humidity in percent |
| 3 | Humidity decimal, currently reserved as zero |
| 4 | DHT11 valid flag |
| 5 | Application major version |
| 6 | Low 8 bits of sample counter |
| 7 | Checksum |

The Linux client marks the STM32 node offline when no valid frame is seen for
more than three seconds. It publishes the latest state atomically to
`/tmp/stm32_can_state.json` and periodically appends samples to CSV.

## `0x201` control command

| Byte 0 | Command | Byte 1 |
|---:|---|---|
| `0x01` | Set LED0 | `0` off, `1` on |
| `0x02` | Set LED1 | `0` off, `1` on |
| `0xA5` | Enter bootloader | Reserved |

The STM32 responds on `0x202`; byte 0 echoes the command and byte 1 is `0` for
success or `1` for unsupported command.

## Physical bus notes

- Use a CAN transceiver at every controller; MCU CAN pins cannot connect
  directly to CANH/CANL.
- Connect CANH, CANL and ground between nodes.
- Fit 120-ohm termination at the two physical ends of the bus.
- Match the nominal bitrate and verify an error-active state with `ip -details
  link show can0` before debugging application frames.
