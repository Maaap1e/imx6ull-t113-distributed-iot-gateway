# STM32F103 CAN OTA Bootloader

本工程基于开发板 IAP Bootloader 例程修改，项目新增和维护的核心入口
为：

- `User/main_can_ota.c`
- `User/CAN_OTA/can_ota.c`
- `../common/ota_metadata.c`
- `../../common/ota_contract.h`

## Flash 与构建

```text
Bootloader  0x08000000 + 0x10000
App         0x08010000 + 0x6F800
Metadata    0x0807F800 + 0x0800
```

Keil 工程已经把 Bootloader IROM 限制为 64 KiB，防止链接输出覆盖 App。
用 ST-Link 烧录 Bootloader 后，使用版本化 `.ota` 包安装 App。

## 启动策略

- 元数据 `PENDING`：镜像校验通过后标记 `TRIAL`，允许一次启动；
- 元数据 `TRIAL`：上次 App 未确认，留在 CAN 恢复模式；
- 元数据 `CONFIRMED`：镜像 CRC 和向量通过后正常启动；
- 元数据为空或损坏：留在恢复模式。

恢复模式可以连续接收新的 ENTER 和 OTA 会话，无需每次失败后重新给
Bootloader 断电。

## CAN 设置

```text
Bitrate: 500 Kbit/s
CAN TX:  PA12
CAN RX:  PA11
```

协议和错误码详见 [`docs/OTA_FLOW.md`](../../docs/OTA_FLOW.md)。
