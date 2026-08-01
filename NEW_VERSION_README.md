# 2.1 独立开发版

本目录是 1.0 当前工作树的完整副本，新增 A/B 自动回滚和 SHA-256 + ECDSA P-256
签名 OTA，并加入按 2KB Flash 页持久化的断点续传。原 1.0 目录和原 Git
暂存区没有被修改。

从这里开始：

- 设计、构建、密钥和验收说明：`docs/OTA_AB_SECURE_V3.md`
- 发布集成说明：`docs/RELEASE_V2.1.0_RC1.md`
- 实板验收证据：`docs/acceptance/v2.1.0-rc.1/RESULT.md`
- Bootloader：`stm32/can_ota_bootloader_ab_secure`
- A/B App：`stm32/dht11_can_app_slot_a`、`stm32/dht11_can_app_slot_b`
- Host：`linux/can_ota_host_ab_secure`
- 打包器：`tools/package_stm32_ota_ab.py`

`stm32/can_ota_bootloader`、`stm32/dht11_can_app` 和
`linux/can_ota_host` 是复制时保留的 v1 参考实现，新功能不依赖修改这些目录。

当前发布候选版本为 `2.1.0-rc.1`。新 Host 已纳入 i.MX6ULL 构建、目标包和
`/opt/iot-gateway/linux/can_ota_host_ab_secure` 持久化安装；旧单槽 Host 继续保留。
