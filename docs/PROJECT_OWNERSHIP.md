# 项目贡献与来源边界

## 目的

本项目用于展示三块真实开发板之间的完整数据链路和工程化能力。它使用了芯片厂商 SDK、开发板例程、LVGL 及既有 T113 界面工程，因此需要把“项目新增工作”“基于例程的适配”和“第三方基础代码”分开说明。

这份文档描述技术贡献范围，不替代各源文件中的许可证或版权声明。第三方许可边界仍以 [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md) 和文件头声明为准。

## 贡献矩阵

| 模块 | 基础来源 | 本项目完成的工作 | 可验证位置 |
|---|---|---|---|
| 系统架构与数据链路 | 自主设计 | 设计 STM32 CAN 节点、i.MX6ULL 边缘网关、T113 显示终端三节点架构，确定 CAN、TCP、状态文件和 LVGL 的职责边界 | [`README.md`](../README.md)、[`TCP_PROTOCOL.md`](TCP_PROTOCOL.md)、[`CAN_PROTOCOL.md`](CAN_PROTOCOL.md) |
| i.MX6ULL 用户态网关 | Linux/POSIX 接口 | 采集本地传感器、合并 STM32 状态、封装自定义 TCP 帧，处理部分收发、心跳、超时、CRC32 和重连 | [`linux/imx6ull_gateway`](../linux/imx6ull_gateway) |
| STM32 CAN 状态服务 | SocketCAN | 定义心跳、DHT11、控制和 ACK 帧，完成过滤、Checksum8、离线判断以及 JSON/CSV 状态输出 | [`linux/can_sensor_client`](../linux/can_sensor_client)、[`stm32/dht11_can_app`](../stm32/dht11_can_app) |
| STM32 CAN IAP/OTA | STM32 HAL、CMSIS 和开发板 BSP | 设计 OTA 会话和状态机，完成 App 分区、向量跳转、6 字节分片、Flash 分块写入、序号检查和整包 CRC32 校验 | [`linux/can_ota_host`](../linux/can_ota_host)、[`stm32/can_ota_bootloader`](../stm32/can_ota_bootloader)、[`OTA_FLOW.md`](OTA_FLOW.md) |
| T113 TCP 接收服务 | POSIX Socket | 实现完整帧接收、CRC 校验、断连处理、JSON/CSV 输出和状态文件原子更新 | [`t113/tcp_receiver`](../t113/tcp_receiver) |
| T113/LVGL 界面 | 既有 T113/LVGL 教学工程和平台组件 | 重构主页面及页面状态逻辑，接入网关数据、Wi-Fi 配置与连接、传感器仪表盘和在线状态；以状态文件解耦网络服务和 UI，并进行定时器及差量刷新优化 | [`page_main.c`](../t113/lvgl_app/ui/page_main.c)、[`page_wifi_setting.c`](../t113/lvgl_app/ui/page_wifi_setting.c)、[`gateway_state.c`](../t113/lvgl_app/ui/data/gateway_state.c)、[`GATEWAY_INTEGRATION.md`](../t113/lvgl_app/GATEWAY_INTEGRATION.md) |
| 运行管理与发布 | BusyBox、systemd、Shell | 增加配置文件、自启动、进程守护、健康检查、日志/CSV 轮转、OTA 互斥、交叉构建和目标板安装包 | [`scripts`](../scripts)、[`deploy`](../deploy)、[`config`](../config)、[`RUNTIME_MANAGEMENT.md`](RUNTIME_MANAGEMENT.md) |
| 测试与实板验收 | 自主设计 | 增加协议单测和故障注入，完成 24 小时运行、CAN/TCP 断开恢复、进程拉起、STM32 OTA 及断电保持验证 | [`tests`](../tests)、[`v1.0.0 验收报告`](acceptance/v1.0.0/RESULT.md) |

## i.MX6ULL 驱动与设备树工作的边界

AP3216C 和 ICM20608 的 i.MX6ULL 内核驱动不是从空白文件开始编写，而是基于官方或开发板例程进行适配。实际工作包括：

- 根据板级连接修改设备树节点、匹配信息和启用状态；
- 调整 I2C/SPI 寄存器访问与设备初始化逻辑；
- 修改字符设备及 `file_operations` 数据通路；
- 编写用户态测试程序验证 `/dev` 读取路径；
- 通过 NFS 共享目录部署模块和测试程序，结合 `dmesg`、设备节点和实测数据定位问题。

完整 Linux BSP/内核树体积较大，且包含板厂代码与许可边界，因此当前仓库没有整体复制。现有用户态采集和容错路径可在 [`linux/imx6ull_gateway/sensors.c`](../linux/imx6ull_gateway/sensors.c) 中查看；调试流程记录在 [`IMX6ULL_DRIVER_PORTING_AND_NFS.md`](IMX6ULL_DRIVER_PORTING_AND_NFS.md)。

## 不作出的声明

- 不把所有 LVGL 页面、图片、字体或教学工程结构声明为独立原创；
- 不把 STM32 HAL、CMSIS、开发板 BSP 或 Linux 厂商 SDK 声明为本项目代码；
- 不把基于例程完成的驱动适配描述为“从零编写完整驱动框架”；
- 不把规划中的 MQTT、固件签名、自动回滚或断点续传描述为已经实现。

项目价值主要体现在跨层集成、协议设计、故障处理、实板联调、验证证据和可解释的工程取舍。
