# v1.2.0-rc.1 STM32 CAN OTA 增量验收

- 验收日期：2026-07-29
- 验收结论：**PASS（本报告所列范围）**
- 最终 STM32 App：`1.2.0.3`
- 目标硬件：STM32F103，Hardware ID `0xF103`
- 验收范围：版本化 OTA 包、CAN 传输、Flash 校验、试启动确认、确认后冷启动保持、DHT11 数据恢复

## 验收环境

- STM32F103：常驻 CAN Bootloader + DHT11 CAN App
- i.MX6ULL：Linux OTA Host、SocketCAN、CAN 传感器客户端和 runtime supervisor
- CAN：500 kbit/s，`can0`
- App 地址：`0x08010000`
- 启动元数据页：`0x0807F800`
- OTA 包：`stm32_dht11_can_app-v1.2.0.3.ota`

## 固件标识

| 产物 | SHA-256 |
|---|---|
| `stm32_dht11_can_app.bin` | `6b0d12b317b30490b8a813d84a0a5e9187b7f1a11db692e92cd09af930022ab1` |
| `stm32_dht11_can_app-v1.2.0.3.ota` | `e8f715055181805b4c67e7d01791ea6e79d321f06cca7b5b80c2a9b7756620e0` |

Host 解析到的最终镜像信息：

```text
hardware=0xF103
version=1.2.0.3
image_size=39720
image_crc32=0xF4BD8235
allow_downgrade=false
```

## 验收结果

| 项目 | 结果 | 证据 |
|---|---|---|
| `.ota` 包解析与 Manifest 发送 | PASS | Host 输出硬件 ID `0xF103`、版本 `1.2.0.3`、镜像长度和 CRC32 |
| CAN 分片写入 | PASS | 写入进度达到 100%，最终序号 6620 |
| STM32 镜像校验 | PASS | 状态依次到达 `verify` 和 `done`，错误码为 `none(0x00)` |
| App 试启动确认 | PASS | Host 收到 `STM32 App confirmed and heartbeat version 1.2.0.3 received.` |
| Host 端最终成功判定 | PASS | 输出 `STM32 CAN OTA and trial-boot confirmation finished successfully.` |
| 确认状态断电保持 | PASS | STM32 与 DHT11 完整断电后仍启动 `1.2.0.3` |
| 冷启动后 DHT11 恢复 | PASS | 6 次连续状态均为 `dht_ok=1`，温度 28°C，湿度 39%～40% |
| CAN 数据连续性 | PASS | 连续记录中 counter 29→34、frames 170→180 |
| CAN 数据校验 | PASS | `checksum_errors=0` |
| CAN 控制器状态 | PASS | 当前状态 ERROR-ACTIVE，berr-counter tx/rx 均为 0，RX/TX errors 和 dropped 均为 0 |
| Linux 进程健康 | PASS | supervisor、CAN client、gateway、MQTT bridge 均为 OK |

## 关键修复及回归

### STM32F1 Flash 状态标记

早期实现尝试在同一 Flash halfword 上依次写入
`PENDING -> TRIAL -> CONFIRMED`。STM32F1 对已经编程过的 halfword
再次编程并不可靠，导致镜像写入和 CRC 校验完成后，App 确认心跳仍可能
超时。

最终实现将 `trial` 和 `confirmed` 拆成两个独立、初始为擦除态的
halfword marker。每个地址只编程一次，最终实板确认和冷启动保持均通过。

### DHT11 上电与有效性状态

开发过程中先后处理了以下问题：

- 避免 `dht11_init()` 后立即再次读取，满足 DHT11 最小采样间隔；
- DHT11 故障采用周期恢复，不阻塞 CAN 心跳和远程进入 Bootloader；
- 使用哨兵值和量程检查拦截厂商读取函数的校验失败假成功；
- 区分接口就绪 `dht_ready` 与有效样本 `dht_ok`，只有取得有效数据后才上报 `dht_ok=1`。

最终冷启动记录中没有再出现 `0/0` 被持续声明为有效数据的情况。

## 证据边界

本次实板记录直接证明：

- 正常版本化 OTA；
- 镜像写入、CRC 校验和 App 版本确认；
- 已确认镜像在完整冷启动后继续运行；
- DHT11、CAN 数据和 Linux 服务在最终版本中恢复正常。

以下能力已有代码或自动化测试覆盖，但**不在本次实板 PASS 声明范围内**：

- 错误 Hardware ID 的实板拒绝；
- 默认降级拒绝和显式授权降级；
- 传输中途断电后重新进入恢复模式；
- App 确认前复位；
- 同一 Bootloader 会话中的失败后重试；
- 数字签名、受保护的防回滚计数、A/B 自动回滚和断点续传。

当前设计是单 App 槽位恢复方案。未确认或损坏镜像不会被反复启动，但旧
镜像被覆盖后不能自动恢复，需要通过常驻 Bootloader 重新发送有效固件。

## 原始证据

- [最终 OTA 传输与确认日志](raw/stm32-ota-v1.2.0.3-transfer.txt)
- [最终冷启动、连续状态、健康检查与 CAN 快照](raw/stm32-ota-v1.2.0.3-final-powercycle.txt)
- [最终固件与 OTA 包 SHA-256](raw/stm32-ota-v1.2.0.3-sha256.txt)
- [1.2.0.1 升级后状态](raw/stm32-ota-v1.2.0.1-after.txt)
- [1.2.0.1 冷启动问题复现](raw/stm32-ota-v1.2.0.1-powercycle.txt)
- [1.2.0.1 OTA 开发记录](raw/stm32-ota-v1.2.0.1.txt)
