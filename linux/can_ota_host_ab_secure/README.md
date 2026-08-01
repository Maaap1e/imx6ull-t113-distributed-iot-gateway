# i.MX6ULL Secure A/B CAN OTA Host

该 Host 仅接受 `tools/package_stm32_ota_ab.py` 生成的 `.ota3` 双镜像包。包中
每个 2KB Flash 页都有被签名保护的 SHA-256，重连后 Host 从 STM32 报告的持久化
页边界继续发送。

```sh
make CC=arm-linux-gnueabihf-gcc
./stm32_can_ota_ab_secure_host \
  -i can0 -f stm32-dht11-v2.1.0.0.ota3
```

推荐使用 `run_ota.sh`，它会在 OTA 期间暂停 CAN 采集服务，完成后恢复。
Bootloader 决定非确认目标槽；Host 发送签名头及目标槽的分块哈希表，再从设备
确认的首个缺失页继续传输，并以版本和槽位都匹配的确认心跳作为最终成功条件。

完整说明见 `../../docs/OTA_AB_SECURE_V3.md`。
