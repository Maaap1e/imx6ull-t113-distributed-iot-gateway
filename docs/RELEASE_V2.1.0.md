# v2.1.0 正式版发布说明

发布日期：2026-08-18

## 发布结论

`v2.1.0` 是本项目当前稳定版本。A/B Secure CAN OTA 将已经通过完整实板验收的
`v2.1.0-rc.1` 功能基线直接转正；候选版之后没有修改 STM32 固件、OTA 协议、
`.ota3` 签名内容或 Linux Secure Host。正式版另外公开了 AP3216C/ICM20608
驱动、最小 DTSI、共享 UAPI 和传感器诊断程序，并纳入构建、安装与自动化测试。

本版可以准确表述为：

> 面向 STM32F103 的带发布者签名、支持自动回滚和页级断点续传的 A/B CAN OTA，
> 并已集成到 i.MX6ULL 分布式物联网网关的构建、安装和运行管理流程。

最终 STM32 App 为 `2.1.0.3`，确认槽为 Slot B。完整断电后 App、DHT11 与 CAN
数据恢复正常，最终记录中 `online=1`、`dht_ok=1`、`checksum_errors=0`。

## 从 1.x 到 2.1.0

| 阶段 | 完成内容 |
|---|---|
| `v1.0.0` | 三节点 CAN/TCP/LVGL 核心链路、启动守护、故障恢复和 24 小时稳定性验收 |
| `v1.1.0` | MQTT 北向桥接、LWT、白名单控制、Qt 总览/趋势/CSV/双语界面和闭环回执 |
| `v1.2.0-rc.1` | 单槽版本化 `.ota`、Hardware ID、四段版本、CRC32、试启动确认和失败后恢复重刷 |
| `v2.1.0-rc.1` | A/B 自动回滚、SHA-256/ECDSA-P256、Key ID、授权降级、双页日志和 2KB 页级续传实板验收 |
| `v2.1.0` | 将上述已验收 OTA 作为正式稳定版本发布，补齐 Linux 传感器驱动、共享 UAPI、诊断工具及统一发布资产 |

## v2.1.0 核心能力

- Slot A/Slot B 独立链接与非确认槽升级；
- `PENDING / TRIAL / CONFIRMED` 试启动确认与自动回滚；
- SHA-256 镜像和页面摘要；
- ECDSA-P256 固件发布者签名；
- Hardware ID 与可信 Key ID 校验；
- 默认拒绝降级及签名保护的 `allow_downgrade`；
- 双页 Boot Metadata 与双页 Resume Journal；
- 2KB Flash 页级持久化断点续传；
- Host 以目标版本和槽位完全匹配的确认心跳作为成功条件；
- OTA 期间暂停 CAN 采集服务，结束或失败后恢复；
- Secure Host 随 i.MX6ULL 运行包持久化安装到 `/opt/iot-gateway`。

Linux 网关侧同时提供 AP3216C/ICM20608 字符设备驱动、传感器 DTSI、共享数据
结构和 `/opt/iot-gateway/linux/sensor_diag/sensor_smoke_test` 诊断程序。

## 实板验收摘要

以下项目均在 `v2.1.0-rc.1` 候选阶段通过，正式版未改变被测功能基线：

- 空白设备首次签名安装到 Slot B；
- Slot B → Slot A 与 Slot A → Slot B 升级；
- TRIAL 镜像确认前复位后自动回滚；
- 约 30% 传输断电后从 `12288/39716` 字节恢复；
- ECDSA 签名、Key ID、Hardware ID 错误在擦除前拒绝；
- 默认降级拒绝、签名授权降级以及恢复最新版；
- STM32/DHT11 完整断电保持；
- i.MX6ULL 正式运行包安装和整机重启恢复。

完整矩阵和原始记录见
[v2.1.0 正式版验收索引](acceptance/v2.1.0/RESULT.md)与
[v2.1.0-rc.1 原始实板验收](acceptance/v2.1.0-rc.1/RESULT.md)。

## 发布资产

正式 Release 建议包含：

```text
iot-gateway-2.1.0-imx6ull.tar.gz
iot-gateway-2.1.0-imx6ull.tar.gz.sha256
stm32_can_ota_ab_secure_bootloader.hex
stm32_dht11_can_app_slot_a-v2.1.0.3.bin
stm32_dht11_can_app_slot_b-v2.1.0.3.bin
stm32-dht11-v2.1.0.3.ota3
v2.1.0-stm32-assets.sha256
v2.1.0-acceptance-evidence.tar.gz
v2.1.0-acceptance-evidence.tar.gz.sha256
```

STM32 三个二进制与 `.ota3` 应直接采用已经验收的 RC 内容，只规范化稳定版清单
名称，不重新修改镜像。正式资产通过以下命令统一收集：

```sh
sh scripts/collect_v2_1_release_assets.sh /path/to/v2.1.0-rc.1-assets
```

脚本直接复用已经发布和校验的 `v2.1.0-rc.1` 验收压缩包，保留其内部证据目录与
内容字节，仅生成稳定版附件名和新的外层 SHA-256。

私钥、私钥口令、故障注入 `.ota3`、验收专用 Host 和 Keil 中间文件不属于公开
发布资产。

## 安装与 OTA

i.MX6ULL 运行包安装：

```sh
tar -xzf iot-gateway-2.1.0-imx6ull.tar.gz
cd iot-gateway-2.1.0-imx6ull
sh scripts/install_target.sh imx6ull
```

安装后执行已签名 OTA：

```sh
/opt/iot-gateway/linux/can_ota_host_ab_secure/run_ota.sh \
  /path/to/stm32-dht11-v2.1.0.3.ota3
```

## 能力边界

本版本是经过发布者签名认证的 A/B CAN OTA，但不声明为完整安全启动、受保护的
密码学防回滚或远程安全 OTA。RDP/WRP、受保护单调计数器、MQTT/TLS 远程固件
下载及更多极端故障注入属于后续产品化方向，不阻塞本项目正式发布。
