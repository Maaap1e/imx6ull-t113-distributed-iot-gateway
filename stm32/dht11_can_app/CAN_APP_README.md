# STM32 DHT11 CAN App

This is the normal STM32 App that runs after `STM32_CAN_OTA_Bootloader`.

## Flash Layout

```text
Bootloader: 0x08000000
App:        0x08010000
```

The Keil project is configured to link the App at `0x08010000`.

## CAN IDs

```text
0x101  STM32 -> i.MX6ULL  heartbeat
0x102  STM32 -> i.MX6ULL  DHT11 data
0x201  i.MX6ULL -> STM32  control command
0x202  STM32 -> i.MX6ULL  control ACK
```

## DHT11 Frame, CAN ID 0x102

```text
DATA[0] = temperature integer
DATA[1] = temperature decimal, DHT11 is normally 0
DATA[2] = humidity integer
DATA[3] = humidity decimal, DHT11 is normally 0
DATA[4] = dht_ok, 1 means valid
DATA[5] = app major version
DATA[6] = counter low byte
DATA[7] = checksum, sum DATA[0..6]
```

## Control Frame, CAN ID 0x201

```text
DATA[0] = command
DATA[1] = value
```

Commands:

```text
0x01 LED0, value 1=on 0=off
0x02 LED1, value 1=on 0=off
0xA5 reset into Bootloader
```

## Build Notes

Open `Projects/MDK-ARM/atk_f103.uvprojx` in Keil and build target `DHT11_CAN_APP`.

Generate a bin file from the output axf/elf before OTA.
