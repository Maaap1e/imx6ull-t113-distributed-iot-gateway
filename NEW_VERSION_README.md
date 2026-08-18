# v2.1.0 正式版说明

`v2.1.0` 是本项目当前稳定版本，将已经通过实板验收的 `v2.1.0-rc.1` OTA 功能基线
直接转正。正式版没有修改 STM32 固件、OTA 协议、签名包或 Linux Host 行为，并
补齐 AP3216C/ICM20608 驱动、共享 UAPI、诊断程序及其构建测试集成，主要
完成版本元数据、README、构建部署说明、发布说明和验收入口的统一。

从这里开始：

- 设计、构建、密钥和验收说明：`docs/OTA_AB_SECURE_V3.md`
- 正式版发布说明：`docs/RELEASE_V2.1.0.md`
- 正式版验收索引：`docs/acceptance/v2.1.0/RESULT.md`
- 原始实板验收证据：`docs/acceptance/v2.1.0-rc.1/RESULT.md`
- Bootloader：`stm32/can_ota_bootloader_ab_secure`
- A/B App：`stm32/dht11_can_app_slot_a`、`stm32/dht11_can_app_slot_b`
- Host：`linux/can_ota_host_ab_secure`
- 打包器：`tools/package_stm32_ota_ab.py`

`stm32/can_ota_bootloader`、`stm32/dht11_can_app` 和 `linux/can_ota_host` 是保留的
v1 单槽参考实现；正式功能使用独立的 A/B Bootloader、Slot A/Slot B App 与 Secure
Host，不覆盖历史实现。

Secure Host 已纳入 i.MX6ULL 构建、正式目标包和
`/opt/iot-gateway/linux/can_ota_host_ab_secure` 持久化安装；旧单槽 Host 继续保留。
