# v2.1.0-rc.1 STM32 A/B Secure CAN OTA 实板验收记录

## 验收结论

**PASS（当前发布声明范围）**：A/B 双槽正常切换、试启动确认、确认前复位自动
回滚、SHA-256/ECDSA-P256 固件认证、Hardware ID/Key ID/版本策略拒绝、受签名
保护的授权降级，以及 STM32 传输断电后的 2KB 页级断点续传均已通过实板验证。
最终确认版本 `2.1.0.3` 在 Slot B 完整断电后正常启动，DHT11 数据有效，CAN
校验错误为 0。

验收日期：2026-08-01（Asia/Shanghai）

本结论不等同于“任意指令点断电均已验证”，也不把当前实现描述为完整安全启动、
受保护的密码学防回滚或远程安全 OTA。覆盖边界见本文末尾。

## 已通过项目

| 项目 | 结果 | 关键证据 |
|---|---|---|
| 空白设备首次签名安装到 Slot B | PASS | `2.1.0.0`、Key ID `0x5A876252`、写入/校验/确认成功 |
| Slot B 完整断电保持 | PASS | 冷启动后 `2.1.0.0`，DHT11 有效，CAN 校验错误为 0 |
| Slot B → Slot A 正常升级 | PASS | `2.1.0.1` 从 Slot A 确认心跳，最终状态正常 |
| Slot A 完整断电保持 | PASS | `active=0 confirmed=0 candidate=255 attempted=0` |
| TRIAL 确认前复位自动回滚 | PASS | 故障 Slot B 未确认，恢复 Slot A `2.1.0.1`，候选状态清除 |
| STM32 约 30% 传输断电续传 | PASS | 第二会话从 `12288/39716` 恢复；`12288 = 6 × 2048` |
| 续传后最终校验与确认 | PASS | Slot B `2.1.0.3`，DHT11 有效，`checksum_errors=0` |
| Header CRC 有效但 ECDSA 签名被篡改 | PASS | 擦除前返回 `ecdsa-signature (0x0D)`，进度和接收量均为 0 |
| 签名拒绝后确认槽保持 | PASS | 复位后仍运行 Slot B `2.1.0.3`，DHT11 和 CAN 正常 |
| 错误 Key ID 拒绝 | PASS | `0x5A876253` 在擦除前返回 `key-id (0x0E)` |
| Key ID 拒绝后确认槽保持 | PASS | 复位后仍运行 Slot B `2.1.0.3`，校验错误为 0 |
| 默认降级拒绝 | PASS | 合法签名 `2.1.0.1` 在擦除前返回 `rollback-policy (0x0A)` |
| 降级拒绝后确认槽保持 | PASS | 复位后仍运行 Slot B `2.1.0.3`，校验错误为 0 |
| `allow_downgrade` 授权降级 | PASS | 受签名保护的授权包成功降级并确认 Slot A `2.1.0.1` |
| 授权降级后恢复最新版 | PASS | 正常签名包重新确认 Slot B `2.1.0.3` |
| 错误 Hardware ID 的 Host 预检查 | PASS | `0xF102` 在发送 CAN 前被生产 Host 拒绝，App 保持在线 |
| 错误 Hardware ID 的 Bootloader 拒绝 | PASS | 独立验收 Host 发送 `0xF102`，STM32 擦除前返回 `hardware-id (0x09)` |
| Hardware ID 拒绝后确认槽保持 | PASS | 复位后仍运行 Slot B `2.1.0.3`，校验错误为 0 |
| 最终完整断电持久化 | PASS | `active=1 confirmed=1 candidate=255 attempted=0`，从 `0x08047000` 启动 Slot B |
| 最终应用与传感器恢复 | PASS | `2.1.0.3`、`online=1`、`dht_ok=1`、`checksum_errors=0` |
| i.MX6ULL 发布包持久化安装 | PASS | 重启后 `/opt/iot-gateway/linux/can_ota_host_ab_secure` 完整存在，Host 为 ARM EABI5 |
| i.MX6ULL 重启后业务自恢复 | PASS | `2.1.0.3`、`online=1`、`dht_ok=1`、`checksum_errors=0` |

## 关键状态变化

```text
空白 Flash
  → 有效签名 2.1.0.0 安装并确认 Slot B
  → 有效签名 2.1.0.1 安装并确认 Slot A
  → 故障 2.1.0.2 试启动未确认并复位
  → 自动回滚并保持 Slot A 2.1.0.1
  → 正常 2.1.0.3 写到约 30% 时切断 STM32 电源
  → 同包从 12288 字节恢复
  → 校验并确认 Slot B 2.1.0.3
  → 错误签名/Key ID/Hardware ID/降级包全部在擦除前拒绝
  → 完整断电后 active=confirmed=Slot B，继续运行 2.1.0.3
```

## 包身份

| 包 | 用途 | SHA-256 |
|---|---|---|
| `stm32-dht11-v2.1.0.0.ota3` | 首次安装 | `8923539ed80faa471f70cc74007d9e85b4b66651a637fef7ef297366ad5c3047` |
| `stm32-dht11-v2.1.0.1.ota3` | Slot A 升级/默认降级拒绝 | `c65a1bc37dedcb791fa036999611d6160556a0d0fdfded1a9462a8f5df18ef54` |
| `stm32-dht11-v2.1.0.1-allow-downgrade.ota3` | 授权降级 | `1d14e9473dfe5909746862c8b57c01de2ffe6f12b74436c10f5b43becfcf186d` |
| `stm32-dht11-v2.1.0.2-rollback-test.ota3` | 未确认试启动回滚 | `50f5bf81c2cf413215588deb63f46c9914e68acd3d69cd5d318004ed6fb9b43a` |
| `stm32-dht11-v2.1.0.3-resume-test.ota3` | 断电续传与最终版本 | `9258e7d6c7ced2a1a8ef77c371a9614cbed901a8e7ec62a24426e2d85c4acd66` |
| `stm32-dht11-v2.1.0.3-invalid-signature.ota3` | ECDSA 拒绝 | `06dc20c83e5eb8291c32a5caf8f98fa783162addb19956a7bb37dc089f30cc41` |
| `stm32-dht11-v2.1.0.3-invalid-key-id.ota3` | Key ID 拒绝 | `298dfd2ad83d4067a9e61dfb392db0e6f077e251df0551e501bff78228bd248e` |
| `stm32-dht11-v2.1.0.3-invalid-hardware-id.ota3` | Hardware ID 拒绝 | `ee8863500492ed274cf22ee3b075a315d9376beb30fb119b8e1f89e8036c4517` |

所有正常接收包的 Hardware ID 为 `0xF103`，可信 Key ID 为 `0x5A876252`。
完整校验清单见 [ota-packages.sha256](raw/ota-packages.sha256)。

## 尚未覆盖的扩展故障注入

- Boot Metadata 双页面恰好轮转时断电。
- Resume journal 双页面恰好轮转时断电。
- 人工篡改已经写完的 Flash 页后重连，从首个哈希不匹配页重新写入。

这些项目不影响本次已验证的 A/B 回滚、签名拒绝和实际传输断电续传结论，但在完成
前不能宣称“任意断电阶段均已实板验证”。

## 安全边界

- 当前是带发布者签名的 CAN OTA，不是完整安全启动；Bootloader 每次启动 App 时
  不重新验证原始 ECDSA 清单。
- 公钥位于普通 Bootloader Flash；本报告不包含 RDP/WRP 等量产保护配置验收。
- 版本比较是签名覆盖的策略控制，不是受保护单调计数器，不能称为密码学防回滚。
- 本次不包含 MQTT/TLS、远程命令鉴权和固件安全下载链路。

## 证据入口

- [证据索引与可追溯关系](EVIDENCE_INDEX.md)
- [原始证据目录](raw/)
- [最终冷启动 UART](raw/final-cold-powercycle-uart.txt)
- [最终冷启动应用状态](raw/final-cold-powercycle-state.json)
- [CAN 接口最终状态](raw/can0-final.txt)
- [i.MX6ULL 发布安装重启保持](raw/release-install-after-imx6ull-reboot.txt)
- [原始证据文件 SHA-256 清单](EVIDENCE_SHA256SUMS.txt)
