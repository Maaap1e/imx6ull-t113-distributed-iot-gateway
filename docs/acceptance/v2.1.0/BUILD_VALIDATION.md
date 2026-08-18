# v2.1.0 正式版构建验证

验证日期：2026-08-18（Asia/Shanghai）。

## 自动化测试

Ubuntu 构建目录执行 `make check`：

- Shell 脚本语法检查通过；
- TCP 协议、MQTT 命令、OTA 包解析、SHA-256、传感器 UAPI 共 5 个 C 测试通过；
- 36 个 Python 测试通过。

## STM32 工程

Windows Keil MDK、ARM Compiler 5.06 update 6 重新构建：

| 工程 | 结果 |
|---|---|
| A/B Secure Bootloader | 0 Error(s), 0 Warning(s) |
| Slot A App | 0 Error(s), 0 Warning(s) |
| Slot B App | 0 Error(s), 0 Warning(s) |

正式 Release 继续使用 `v2.1.0-rc.1` 已完成实板验收的 Bootloader、Slot A/Slot B
二进制和正常签名 `.ota3`，本次重编译产物只用于验证工程仍可构建，不替换已验收资产。

## i.MX6ULL 交叉构建

使用 GCC Linaro 4.9.4 `arm-linux-gnueabihf-gcc` 完成以下目标：

- `imx6ull_gateway_app`；
- `stm32_can_sensor_client`；
- 单槽 `stm32_can_ota_host`；
- `stm32_can_ota_ab_secure_host`；
- `sensor_smoke_test`；
- `mqtt_bridge`。

上述产物均确认为 ARM 32-bit EABI5 可执行文件。

## 正式发布资产

生成目录包含 9 个附件：

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

三个 SHA-256 清单均校验通过。正式版验收包直接复用RC验收包内容，两者SHA-256
均为：

```text
834e154419ba62e6528d97ffc74f981c6ae12a3ce09b7af34133bc03ccf6ad74
```
