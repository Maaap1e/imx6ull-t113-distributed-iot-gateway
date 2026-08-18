# i.MX6ULL AP3216C / ICM20608 驱动适配

本目录保存从开发板例程整理出的 AP3216C I2C 驱动、ICM20608 SPI 驱动、
传感器设备树片段和独立诊断程序。

| 文件 | 作用 |
|---|---|
| `ap3216c.c` | AP3216C 字符设备驱动，输出 IR/ALS/PS 原始值 |
| `icm20608.c` | ICM20608 字符设备驱动，输出陀螺仪、加速度和温度原始值 |
| `sensor_uapi.h` | 驱动、网关和诊断程序共用的数据结构 |
| `sensor_smoke_test.c` | 脱离 TCP、MQTT 和 UI 的板端连续读取工具 |
| `imx6ull-sensors.dtsi` | I2C1、ECSPI3、片选与 pinctrl 的最小设备树片段 |

主要改动包括：

- 使用 `spi_write_then_read()` 完成 ICM20608 连续寄存器读取，修复旧实现中
  1 字节发送缓冲区参与多字节传输的问题；
- 完整返回 I2C/SPI、缓冲区长度和 `copy_to_user()` 错误；
- 在 `probe()` 完成传感器初始化，增加互斥保护和设备资源释放；
- 统一字符设备 ABI，并用 `tests/sensor_uapi_test.c` 检查结构体大小与字段偏移。

## 构建与使用

针对目标板内核构建模块：

```sh
make KERNEL_DIR=/path/to/imx6ull-linux-kernel \
     ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
```

只构建诊断程序：

```sh
make user-test USER_CC=arm-linux-gnueabihf-gcc
```

通过项目安装包部署后运行：

```sh
/opt/iot-gateway/linux/sensor_diag/sensor_smoke_test -n 10 -d 500
```

字符设备 ABI：

- `/dev/ap3216c`：6 字节，依次为 `u16 ir, als, ps`；
- `/dev/icm20608`：28 字节，依次为 3 轴陀螺仪、3 轴加速度和温度的 `s32` 原始值。

设备树片段来自项目实板 DTS。合入 BSP 时确认 UART2 引脚复用为 ECSPI3、
GPIO1_IO20 作为片选，并避免和已有节点重名。
