# DHT11 CAN App — Slot A

该工程固定链接到 `0x08010000`，最大 220KB。核心 CAN 初始化完成后，它只在
Metadata 中的 candidate、active、版本和编译槽位全部匹配时确认 TRIAL；后续
冷启动确认操作幂等。心跳第 7 字节（索引 6）报告 Slot A。

完整说明见 [`../../docs/OTA_AB_SECURE_V3.md`](../../docs/OTA_AB_SECURE_V3.md)。
