# 项目贡献与来源边界

## 目的

本项目用于展示三块真实开发板之间的完整数据链路和工程化能力。工程使用芯片厂商 SDK、开发板 BSP、LVGL 及 T113 平台组件，因此按“项目功能开发”“板级适配”和“第三方基础组件”说明贡献边界。

这份文档描述技术贡献范围，不替代各源文件中的许可证或版权声明。第三方许可边界仍以 [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md) 和文件头声明为准。

## 贡献矩阵

| 模块 | 基础来源 | 本项目完成的工作 | 可验证位置 |
|---|---|---|---|
| 系统架构与数据链路 | 自主设计 | 设计 STM32 CAN 节点、i.MX6ULL 边缘网关、T113 显示终端三节点架构，确定 CAN、TCP、状态文件和 LVGL 的职责边界 | [`README.md`](../README.md)、[`TCP_PROTOCOL.md`](TCP_PROTOCOL.md)、[`CAN_PROTOCOL.md`](CAN_PROTOCOL.md) |
| i.MX6ULL 用户态网关 | Linux/POSIX 接口 | 采集本地传感器、合并 STM32 状态、封装自定义 TCP 帧，处理部分收发、心跳、超时、CRC32 和重连 | [`linux/imx6ull_gateway`](../linux/imx6ull_gateway) |
| i.MX6ULL 传感器驱动适配 | ALIENTEK/板厂 Linux 例程与 NXP BSP | 适配 AP3216C/ICM20608 设备树和字符设备，修复 SPI 连续读取缓冲区问题，统一共享 UAPI，增加诊断程序和 ABI 测试 | [`linux/kernel_drivers/imx6ull_sensors`](../linux/kernel_drivers/imx6ull_sensors) |
| STM32 CAN 状态服务 | SocketCAN | 定义心跳、DHT11、控制和 ACK 帧，完成过滤、Checksum8、离线判断以及 JSON/CSV 状态输出 | [`linux/can_sensor_client`](../linux/can_sensor_client)、[`stm32/dht11_can_app`](../stm32/dht11_can_app) |
| STM32 CAN IAP/OTA | STM32 HAL、CMSIS、micro-ecc 和开发板 BSP | 从单槽版本化 OTA 演进到 A/B 双槽，实现试启动确认、自动回滚、ECDSA-P256/SHA-256 发布者认证和页级断点续传 | [`linux/can_ota_host_ab_secure`](../linux/can_ota_host_ab_secure)、[`stm32/can_ota_bootloader_ab_secure`](../stm32/can_ota_bootloader_ab_secure)、[`OTA_AB_SECURE_V3.md`](OTA_AB_SECURE_V3.md) |
| T113 TCP 接收服务 | POSIX Socket | 实现完整帧接收、CRC 校验、断连处理、JSON/CSV 输出和状态文件原子更新 | [`t113/tcp_receiver`](../t113/tcp_receiver) |
| T113/LVGL 本地终端 | T113 平台工程、LVGL 和页面基础组件 | 负责终端功能开发与系统集成，实现 TCP 状态接收、状态文件解耦、网关与 STM32 节点在线判断、传感器仪表盘、Wi-Fi 配置及界面差量刷新 | [`page_main.c`](../t113/lvgl_app/ui/page_main.c)、[`page_wifi_setting.c`](../t113/lvgl_app/ui/page_wifi_setting.c)、[`gateway_state.c`](../t113/lvgl_app/ui/data/gateway_state.c)、[`GATEWAY_INTEGRATION.md`](../t113/lvgl_app/GATEWAY_INTEGRATION.md) |
| 运行管理与发布 | BusyBox、systemd、Shell | 增加配置文件、自启动、进程守护、健康检查、日志/CSV 轮转、OTA 互斥、交叉构建和目标板安装包 | [`scripts`](../scripts)、[`deploy`](../deploy)、[`config`](../config)、[`RUNTIME_MANAGEMENT.md`](RUNTIME_MANAGEMENT.md) |
| 测试与实板验收 | 自主设计 | 增加协议、密码学和 ABI 测试及故障注入，完成 24 小时运行、CAN/TCP 恢复、A/B 回滚、签名拒绝和断电续传验证 | [`tests`](../tests)、[`docs/acceptance`](acceptance) |

## i.MX6ULL 驱动与设备树工作的边界

AP3216C 和 ICM20608 的 i.MX6ULL 内核驱动不是从空白文件开始编写，而是基于官方或开发板例程进行适配。实际工作包括：

- 根据板级连接修改设备树节点、匹配信息和启用状态；
- 调整 I2C/SPI 寄存器访问与设备初始化逻辑；
- 修改字符设备及 `file_operations` 数据通路；
- 编写用户态测试程序验证 `/dev` 读取路径；
- 通过 NFS 共享目录部署模块和测试程序，结合 `dmesg`、设备节点和实测数据定位问题。

完整 Linux BSP/内核树没有整体复制。项目适配后的最小驱动、DTSI、共享 UAPI 与诊断程序位于 [`linux/kernel_drivers/imx6ull_sensors`](../linux/kernel_drivers/imx6ull_sensors)，网关读取路径位于 [`linux/imx6ull_gateway/sensors.c`](../linux/imx6ull_gateway/sensors.c)。

## 不作出的声明

- 不把芯片 SDK、平台 BSP、LVGL、基础页面、图片或字体等第三方组件声明为项目自研；
- 不把 STM32 HAL、CMSIS、开发板 BSP 或 Linux 厂商 SDK 声明为本项目代码；
- 不把基于例程完成的驱动适配描述为“从零编写完整驱动框架”；
- 不把发布者签名 OTA 描述为完整安全启动或受保护的密码学防回滚。

项目价值主要体现在跨层集成、协议设计、故障处理、实板联调、验证证据和可解释的工程取舍。
