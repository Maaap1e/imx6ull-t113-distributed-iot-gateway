# A/B Secure CAN OTA Bootloader

该工程保留 64KB Bootloader，使用 220KB Slot A、220KB Slot B、两个 2KB
启动 Metadata 日志页和两个 2KB 断点日志页。它在擦除非确认槽之前验证
SHA-256 + ECDSA P-256 签名，并实现按 2KB 页持久化续传、一次 TRIAL、
App 确认和未确认自动回滚。

构建前必须用项目密钥生成工具替换无效公钥占位文件。完整布局、密钥、打包、
迁移和实板验收步骤见 [`../../docs/OTA_AB_SECURE_V3.md`](../../docs/OTA_AB_SECURE_V3.md)。
