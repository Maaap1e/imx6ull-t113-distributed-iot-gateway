# v2.1.0-rc.1 发布说明

## 发布结论

本候选版将 STM32F103 CAN OTA 从单槽恢复模式升级为：

- A/B 双槽试启动确认与自动回滚；
- SHA-256 + ECDSA-P256 发布者签名认证；
- Hardware ID、Key ID、版本与授权降级策略；
- 2KB Flash 页级持久化断点续传；
- Linux Host 以目标槽和版本完全匹配的确认心跳作为成功条件。

最终实板版本为 `2.1.0.3`，确认槽为 Slot B。完整断电后 Bootloader 状态为
`active=1 confirmed=1 candidate=255 attempted=0`，App、DHT11 和 CAN 数据恢复正常。
安全 OTA Host 已通过正式 i.MX6ULL 运行包安装到 `/opt/iot-gateway`，并验证整机
重启后程序、启动脚本和传感器业务均能持久化恢复。
完整测试矩阵和证据见
[v2.1.0-rc.1 验收报告](acceptance/v2.1.0-rc.1/RESULT.md)。

## i.MX6ULL 发布集成

`scripts/build_all.sh imx6ull` 会同时构建旧单槽 Host 和新 A/B Secure Host。
`scripts/package_target.sh imx6ull` 会把新 Host 纳入目标包，安装位置为：

```text
/opt/iot-gateway/linux/can_ota_host_ab_secure/
```

安装后执行：

```sh
/opt/iot-gateway/linux/can_ota_host_ab_secure/run_ota.sh \
  /path/to/stm32-dht11-v2.1.0.3.ota3
```

包装脚本会协调 CAN 采集服务，并在退出时恢复原先运行的客户端。旧版
`/opt/iot-gateway/linux/can_ota_host/` 保留，用于历史兼容，不会被新 Host 覆盖。

## 发布资产与密钥规则

正常发布资产应包含 i.MX6ULL/T113 运行包、三个 STM32 固件产物、一个正常签名
`.ota3` 和验收证据压缩包。不要发布：

- ECDSA 私钥或私钥口令；
- 故障注入 `.ota3`；
- 带 `CAN_OTA_ACCEPTANCE_TEST` 的 Host；
- Keil `Output/` 中的调试中间文件。

生产私钥应保存在仓库和板卡之外的离线签名环境。i.MX6ULL 只需要已签名 `.ota3`，
不需要也不应持有私钥。

Windows 工作区的 `dist/` 已整理以下实板对应资产：

```text
stm32_can_ota_ab_secure_bootloader.hex
stm32_dht11_can_app_slot_a-v2.1.0.3.bin
stm32_dht11_can_app_slot_b-v2.1.0.3.bin
stm32-dht11-v2.1.0.3.ota3
v2.1.0-rc.1-stm32-assets.sha256
```

其中规范化命名的 `.ota3` 与实板断电续传测试包字节完全一致，SHA-256 为
`9258e7d6c7ced2a1a8ef77c371a9614cbed901a8e7ec62a24426e2d85c4acd66`；它只是
重命名复制，没有重新签名。

验收证据归档命令：

```sh
sh scripts/package_acceptance.sh
```

脚本会先按 `EVIDENCE_SHA256SUMS.txt` 校验所有原始证据，再在 `dist/` 生成
`v2.1.0-rc.1-acceptance-evidence.tar.gz` 及对应 `.sha256`。

## 准确能力边界

本版可以表述为“带发布者签名、可断点续传、可自动回滚的 A/B CAN OTA”。它仍
不是完整安全启动，也没有受保护单调计数器、远程 TLS/命令鉴权和固件安全下载。
三项扩展故障注入尚未覆盖：Metadata 页面轮转断电、Resume 页面轮转断电、已完成
Flash 页人工篡改后的哈希恢复。
