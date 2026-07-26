# v1.1.0-rc.1 MQTT 与 Qt 上位机增量验收

- 验收日期：2026-07-26
- 验收结论：**PASS**
- 正式记录区间：16:38:08—19:46:10（3 小时 8 分 2 秒）
- 验收范围：MQTT 北向桥接、Windows Qt 上位机、CSV 记录、LED 命令闭环

## 验收环境

- STM32F103：CAN 传感器节点，App v1.1
- i.MX6ULL：传感器汇聚、TCP Client、MQTT Bridge
- T113：TCP Server 与 LVGL 数据显示
- PC：Windows 11、Qt 5.12.9、Paho MQTT C
- Broker：`mq.tongxinmao.com:18830`
- 遥测主题：`/public/TEST/Maaap1e/imx6ull-01/telemetry`
- 命令主题：`/public/TEST/Maaap1e/imx6ull-01/cmd/led`
- 回执主题：`/public/TEST/Maaap1e/imx6ull-01/cmd/response`

## 验收结果

| 项目 | 结果 | 证据 |
|---|---|---|
| 连续运行 | PASS | 正式记录区间为 3 小时 8 分 2 秒 |
| MQTT 连接 | PASS | `connection_lost=0` |
| MQTT 发布 | PASS | `publish_failed=0`，发布计数达到 3004 |
| Qt 数据接收 | PASS | 截图中累计消息超过 2340 条，解析错误为 0 |
| 趋势数据 | PASS | Qt 内存中已保存 2334 个遥测点 |
| CSV 记录 | PASS | 测试期间持续写入 PC 端 CSV |
| 进程健康 | PASS | supervisor、CAN client、gateway、MQTT bridge 全部为 OK |
| TCP 链路 | PASS | 结束状态为 connected，`disconnect_count=0` |
| CAN 总线 | PASS | ERROR-ACTIVE，RX 错误、丢包、overrun 和 bus-off 均为 0 |
| STM32 节点 | PASS | 在线，固件 v1.1，`checksum_errors=0` |
| LED 控制闭环 | PASS | 熄灭、常亮、呼吸灯命令均收到成功回执，并完成结束复测 |
| 结束时资源状态 | PASS | `/tmp` 使用率 1%，可用内存约 310 MB |

## 关键结果说明

### MQTT 运行状态

结束记录中的 MQTT 事件统计为：

- connected：1
- connection lost：0
- connect failed：2
- publish failed：0

两次连接失败发生在启动阶段，程序随后按退避策略自动重连成功。验收记录中没有连接丢失或发布失败。

### Qt 上位机

Qt 上位机在测试期间保持连接，持续接收和解析真实遥测数据，并完成以下验证：

- 总览页显示 i.MX6ULL、T113 TCP 链路和 STM32 CAN 节点在线；
- 趋势页持续更新，保存 2334 个采样点；
- 控制页在测试开始和结束时均完成 LED 熄灭、常亮、呼吸灯闭环控制；
- 命令发布后收到设备成功回执；
- 解析错误计数为 0；
- PC 端 CSV 持续写入。

验收时的趋势页面最多显示最近 60 分钟，但应用内存中已保存 2334 个遥测点。按约 5 秒一个采样点估算，对应约 3 小时 14 分钟的数据。后续代码已增加 180 分钟趋势窗口；这一界面增强不改变本次连续运行结论。

### CAN 与设备状态

结束时 CAN 控制器处于 ERROR-ACTIVE 状态，错误计数、丢包和 bus-off 均为 0。STM32 节点在线，固件版本为 v1.1，累计帧数为 22540，校验错误为 0。AP3216C、ICM20608 和 DHT11 状态均正常。

## 证据边界

`mqtt-qt-soak-start.txt` 只成功记录了 PC 开始时间戳，未保存起始端的进程、内存和 CAN 快照。因此：

- 可以依据开始/结束时间戳、Qt 控制记录、累计消息与遥测点证明约 3 小时连续功能运行；
- 可以证明结束时系统资源和链路状态正常；
- 本次记录不用于宣称精确的起止内存增长量或日志增长量。

该证据缺口不影响本次 MQTT 与 Qt 功能验收结论，但后续长稳测试应在开始时同时保存完整板端快照。

## 已知限制

- 当前使用公共 MQTT 测试 Broker，不适合生产环境；
- 当前 MQTT 链路未启用 TLS 与身份认证；
- 趋势历史保存在进程内存中，应用重启后从零开始，原始数据另存为 CSV；
- 设备端 epoch/时区差异作为已知问题保留，Qt 图表使用 PC 接收时间；
- 本次开始记录缺少板端完整健康快照，无法计算精确资源增量。

## 验收证据

- [开始时间戳](raw/mqtt-qt-soak-start.txt)
- [结束快照](raw/mqtt-qt-soak-end.txt)
- [Qt 总览页](images/qt-overview-after-3h.png)
- [Qt 趋势页](images/qt-trend-after-3h.png)
- [Qt LED 控制闭环](images/qt-led-control-after-3h.png)
