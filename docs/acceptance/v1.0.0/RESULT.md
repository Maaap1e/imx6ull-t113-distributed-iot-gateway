# v1.0.0 实板验收报告

## 结论

**PASS（通过，带已记录的已知限制）**

验收平台由 STM32F103、i.MX6ULL 和 T113 三块实板组成，数据链路为：

STM32 CAN 节点 → i.MX6ULL 边缘网关 → TCP → T113/LVGL 终端。

## 验收结果

| 项目 | 结果 |
|---|---|
| Linux 双板冷启动与服务自启 | PASS |
| AP3216C、ICM20608、DHT11 数据采集 | PASS |
| CAN/TCP 数据传输与 LVGL 显示 | PASS |
| 连续运行 24 小时 7 分钟 | PASS |
| Ethernet 断线检测与自动重连 | PASS |
| STM32/CAN 离线检测与自动恢复 | PASS |
| T113 接收进程崩溃自动拉起 | PASS |
| i.MX6ULL 网关进程崩溃自动拉起 | PASS |
| STM32 CAN OTA 1.0 → 1.1 | PASS |
| OTA 后 STM32 断电重启 | PASS |

## 关键数据

- 24 小时测试期间 T113 新增接收约 201085 帧。
- TCP CRC 错误为 0，协议错误为 0。
- CAN 无 dropped、bus-off 和校验错误。
- `/tmp` 日志和 CSV 轮转正常，未发生无界增长。
- 未发现 OOM、段错误、系统卡死或无法恢复的连接中断。
- STM32 OTA 固件大小：37224 bytes。
- STM32 OTA CRC32：`0xBE26F076`。
- STM32 固件 SHA-256：
  `0150cb9565d50accdadaa8d71066be945f7cb081b3c88f1e093479746beff232`。
- OTA 后版本为 1.1，STM32 断电重启后仍正常运行 1.1。

## 已知限制

- SSH 非登录环境执行 OTA 时需要将 `/sbin` 加入 `PATH`。
- STM32 counter 的 8 位与 16 位显示口径尚未统一，但不影响在线判断和数据采集。
- i.MX6ULL payload 时间存在已知的 +8 小时差异；T113 外层接收时间正常。
- OTA 使用 CRC32 完整性校验，暂不支持数字签名、回滚和断电续传。
- CAN 客户端启动脚本会输出较多历史日志。
- LVGL 应用与网关服务当前分别管理。

## 原始证据

- [24 小时开始记录](raw/soak-start.txt)
- [24 小时结束记录](raw/soak-end.txt)
- [最终状态](raw/final-acceptance.txt)
- [TCP 断线前](raw/tcp-recovery-before.txt)
- [TCP 断线期间](raw/tcp-recovery-disconnected.txt)
- [TCP 恢复后](raw/tcp-recovery-restored.txt)
- [CAN 断线前](raw/can-recovery-before.txt)
- [CAN 断线期间](raw/can-recovery-disconnected.txt)
- [CAN 恢复后](raw/can-recovery-restored.txt)
- [T113 进程恢复](raw/t113-process-recovery-restored.txt)
- [i.MX6ULL 进程恢复](raw/imx6ull-process-recovery-restored.txt)
- [OTA 升级前](raw/stm32-ota-before.txt)
- [OTA 首次 PATH 失败记录](raw/stm32-ota-transfer.txt)
- [OTA 成功传输记录](raw/stm32-ota-transfer-retry.txt)
- [OTA 升级后](raw/stm32-ota-after.txt)
- [OTA 断电重启验证](raw/stm32-ota-powercycle.txt)
