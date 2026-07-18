# i.MX6ULL STM32 CAN OTA Host

This tool runs on i.MX6ULL Linux and sends a STM32 App binary to the STM32 CAN OTA Bootloader.

## Build

```sh
cd imx6ull_can_ota_host
make
```

Cross compile:

```sh
make CC=arm-linux-gnueabihf-gcc
```

## Bring Up CAN

```sh
chmod +x setup_can.sh
./setup_can.sh can0 500000
```

Equivalent manual commands:

```sh
ip link set can0 down
ip link set can0 type can bitrate 500000
ip link set can0 up
```

## Run OTA

Burn `STM32_CAN_OTA_Bootloader` to STM32 first. Then run:

```sh
./stm32_can_ota_host -i can0 -f stm32_dht11_app.bin
```

Optional pacing:

```sh
./stm32_can_ota_host -i can0 -f stm32_dht11_app.bin -p 20000
```

One-shot script:

```sh
chmod +x setup_can.sh run_ota.sh
./run_ota.sh stm32_dht11_app.bin
```

For the current STM32 bootloader, start the OTA command first and then reset the
STM32 board. The host sends the enter command repeatedly for a few seconds, so it
can catch the bootloader's short OTA window.

## Protocol

```text
0x300  i.MX6ULL -> STM32  enter OTA, DATA[0] = 0xA5
0x301  i.MX6ULL -> STM32  firmware size + crc32
0x302  i.MX6ULL -> STM32  sequence + 6 firmware bytes
0x380  STM32    -> i.MX6ULL status/progress
```

The STM32 App must be linked at `0x08010000`.
