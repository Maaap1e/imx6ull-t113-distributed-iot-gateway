# v2.1.0-rc.1 证据索引

本索引把发布声明映射到原始实板记录。原始文件是串口、i.MX6ULL Host 和
`/tmp/stm32_can_state.json` 的现场摘录；包身份由本地发布工作区重新计算 SHA-256。

## 正常升级与持久化

| 声明 | 原始证据 |
|---|---|
| 首次安装并确认 Slot B `2.1.0.0` | [slot-b-first-install.txt](raw/slot-b-first-install.txt) |
| B→A 安装并确认 `2.1.0.1` | [slot-a-upgrade.txt](raw/slot-a-upgrade.txt) |
| 最终恢复并确认 Slot B `2.1.0.3` | [restore-latest-2.1.0.3-host.txt](raw/restore-latest-2.1.0.3-host.txt)、[状态 JSON](raw/restore-latest-2.1.0.3-state.json) |
| 完整断电后 A/B Metadata 与跳转正确 | [final-cold-powercycle-uart.txt](raw/final-cold-powercycle-uart.txt) |
| 完整断电后 App/DHT11/CAN 正常 | [final-cold-powercycle-state.json](raw/final-cold-powercycle-state.json) |
| i.MX6ULL 重启后 Secure Host 持久存在且业务自恢复 | [release-install-after-imx6ull-reboot.txt](raw/release-install-after-imx6ull-reboot.txt) |

## 回滚与断点续传

| 声明 | 原始证据 |
|---|---|
| TRIAL 确认前复位后回滚到确认槽 | [rollback-final-state.txt](raw/rollback-final-state.txt) |
| 传输约 30% 时先切断 STM32 电源 | [resume-power-loss.txt](raw/resume-power-loss.txt) |
| 同包从 `12288/39716` 页边界恢复 | [resume-power-loss.txt](raw/resume-power-loss.txt) |
| 续传后确认 `2.1.0.3` | [resume-final-state.txt](raw/resume-final-state.txt) |

## 认证与策略拒绝

| 声明 | 拒绝证据 | 旧确认槽保持证据 |
|---|---|---|
| ECDSA 签名篡改拒绝 | [invalid-signature-host.txt](raw/invalid-signature-host.txt) | [JSON](raw/invalid-signature-preserved-state.json) |
| 错误 Key ID 拒绝 | [invalid-key-id-host.txt](raw/invalid-key-id-host.txt) | [JSON](raw/invalid-key-id-preserved-state.json) |
| Host 错误 Hardware ID 预检查 | [invalid-hardware-id-host-precheck.txt](raw/invalid-hardware-id-host-precheck.txt) | [JSON](raw/invalid-hardware-id-host-preserved-state.json) |
| Bootloader 错误 Hardware ID 拒绝 | [invalid-hardware-id-bootloader.txt](raw/invalid-hardware-id-bootloader.txt) | [JSON](raw/invalid-hardware-id-bootloader-preserved-state.json) |
| 默认降级拒绝 | [downgrade-denied-host.txt](raw/downgrade-denied-host.txt) | [JSON](raw/downgrade-denied-preserved-state.json) |
| 受签名保护的授权降级 | [allow-downgrade-host.txt](raw/allow-downgrade-host.txt) | [JSON](raw/allow-downgrade-state.json) |

## 完整性清单

- [全部 OTA 测试包 SHA-256](raw/ota-packages.sha256)
- [全部原始证据文件 SHA-256](EVIDENCE_SHA256SUMS.txt)
- [最终 CAN 接口统计](raw/can0-final.txt)

用于绕过 Host Hardware ID 预检查的 `-H` 仅属于独立验收构建。正式发布 Host
未启用 `CAN_OTA_ACCEPTANCE_TEST`，不会包含该选项；STM32 Bootloader 本身仍执行
Hardware ID 强制校验。
