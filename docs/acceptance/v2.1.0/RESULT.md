# v2.1.0 正式版验收结论

## 结论

**PASS（正式发布声明范围）**。

`v2.1.0` 的 A/B Secure CAN OTA 由已经通过实板验收的 `v2.1.0-rc.1` 原功能基线
直接转正。候选版之后没有修改 STM32 Bootloader、Slot A/Slot B App、OTA 协议、
正常签名 `.ota3` 或 Linux Secure Host，因此 OTA 正式版继承同一组实板证据。

正式版新增公开的 i.MX6ULL 传感器驱动、共享 UAPI 与诊断工具由源码审查和自动化
测试覆盖；既有 AP3216C/ICM20608 板端采集结论见 v1.0.0 验收记录。

正式版构建、测试和发布资产校验见 [BUILD_VALIDATION.md](BUILD_VALIDATION.md)。

正式版文档与版本元数据更新时间：2026-08-18（Asia/Shanghai）。

## 被验收基线

| 项目 | 结果 |
|---|---|
| 最终 STM32 App | `2.1.0.3` |
| 最终活动/确认槽 | Slot B |
| A/B 双向升级与试启动确认 | PASS |
| 未确认镜像自动回滚 | PASS |
| SHA-256/ECDSA-P256 固件认证 | PASS |
| Hardware ID / Key ID / 版本策略 | PASS |
| 签名保护的授权降级 | PASS |
| 2KB 页级断点续传 | PASS，从 `12288/39716` 字节恢复 |
| STM32/DHT11 完整断电保持 | PASS |
| i.MX6ULL 安装与重启恢复 | PASS |
| 最终数据状态 | `online=1`、`dht_ok=1`、`checksum_errors=0` |

## 原始证据

- [完整实板验收矩阵](../v2.1.0-rc.1/RESULT.md)
- [证据索引](../v2.1.0-rc.1/EVIDENCE_INDEX.md)
- [证据 SHA-256 清单](../v2.1.0-rc.1/EVIDENCE_SHA256SUMS.txt)
- [原始记录目录](../v2.1.0-rc.1/raw/)

正式版证据压缩包保留上述 RC 目录名，以明确记录产生时的候选版本身份；压缩包
本身使用 `v2.1.0-acceptance-evidence.tar.gz` 稳定版名称。

## 声明边界

本结论支持“带发布者签名、可自动回滚、可页级断点续传的 A/B CAN OTA”表述。
它不扩展为完整安全启动、受保护单调计数器、远程 TLS OTA 或任意指令点断电均已
验证。详细边界仍以原始验收报告为准。
