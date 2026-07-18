# STM32F103 CAN OTA Bootloader

This folder is a modified copy of `IAP Bootloader V1.0`.

The original serial IAP files are kept for reference. The Keil project is changed to build:

- `User/main_can_ota.c`
- `User/CAN_OTA/can_ota.c`

## Flash Layout

```text
Bootloader: 0x08000000
App:        0x08010000
```

The DHT11 App must be linked at `0x08010000`, and its vector table offset must be `0x10000`.

## CAN Settings

```text
Bitrate: 500 Kbps
CAN TX:  PA12
CAN RX:  PA11
```

STM32 elite board jumper P6 must connect PA12 to CTX and PA11 to CRX.

## Protocol

```text
0x300  i.MX6ULL -> STM32  enter OTA, DATA[0] = 0xA5
0x301  i.MX6ULL -> STM32  firmware info
0x302  i.MX6ULL -> STM32  firmware data
0x380  STM32    -> i.MX6ULL status
```

Firmware info frame:

```text
DATA[0..3] = firmware size, little endian
DATA[4..7] = firmware crc32, little endian
```

Firmware data frame:

```text
DATA[0..1] = sequence, little endian
DATA[2..7] = 6 firmware bytes
```

Status frame:

```text
DATA[0] = status
DATA[1] = error
DATA[2] = progress, 0-100
DATA[3..4] = sequence, little endian
DATA[5..6] = received KB, little endian
```

## Next Step

Build this Bootloader in Keil and burn it once with ST-Link. Then implement the i.MX6ULL CAN OTA host to send `stm32_app.bin`.
