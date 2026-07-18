# i.MX6ULL STM32 CAN Sensor Client

This client receives STM32 DHT11 data through SocketCAN.

## Build

```sh
cd imx6ull_stm32_can_client
make
```

Cross compile:

```sh
make CC=arm-linux-gnueabihf-gcc
```

## Run

```sh
chmod +x setup_can.sh start_client.sh stop_client.sh
./start_client.sh
```

Manual:

```sh
./setup_can.sh can0 500000
./stm32_can_sensor_client -i can0
```

Output files:

```text
/tmp/stm32_can_state.json
/tmp/stm32_can_data.csv
/tmp/stm32_can_client.log
```

## Test Control

Turn LED0 on:

```sh
./stm32_can_sensor_client -i can0 -L 0:1
```

Ask STM32 App to reset into Bootloader:

```sh
./stm32_can_sensor_client -i can0 -b
```

## CAN Frames

```text
0x101 heartbeat
0x102 DHT11 temperature/humidity
0x201 control command
0x202 control ACK
```
