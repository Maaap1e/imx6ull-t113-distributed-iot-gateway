# STM32 DHT11 CAN App

该 App 链接到 `0x08010000`，`v1.2.0-rc.1` 最终实板验收版本为
`1.2.0.3`。Keil 工程将
App 最大长度限制为 `0x6F800`，Flash 最后一页由 OTA 启动元数据占用。

## OTA 启动确认

App 完成时钟、串口、LCD 和 CAN 初始化后：

1. 检查 Bootloader Manifest 版本是否与编译时版本一致；
2. 将启动状态从 `TRIAL` 更新为 `CONFIRMED`；
3. 再进入 DHT11 初始化和周期采集。

确认点位于 DHT11 重试之前，因为 DHT11 缺失不应导致核心 CAN 固件被
判定为启动失败。若 App 在确认前复位，Bootloader 下次启动会进入恢复
模式而不是反复启动该镜像。

DHT11 初始化和故障恢复采用周期性重试，不使用阻塞式无限循环；因此
传感器缺失时 CAN 心跳、控制命令和远程进入 Bootloader 仍保持可用。

DHT11 采样周期为 2 秒。状态机区分接口是否可访问的 `dht_ready` 与
最近一次样本是否有效的 `dht_ok`：初始化成功只代表接口就绪，只有在
读取结果通过哨兵值、温度和湿度量程检查后才置 `dht_ok=1`。读取失败
时继续上报心跳并定期重试，避免把 `0/0` 或校验失败数据声明为有效样本。

`OTA_CONFIRM_DELAY_MS` 是默认值为 `0u` 的验收测试钩子。需要验证确认
前复位时，可临时设为 `10000u` 并重新构建，在延迟窗口内复位；正式
发布构建必须恢复为 `0u`。

## CAN IDs

```text
0x101  STM32 -> i.MX6ULL  heartbeat
0x102  STM32 -> i.MX6ULL  DHT11 data
0x201  i.MX6ULL -> STM32  control command
0x202  STM32 -> i.MX6ULL  control ACK
0x300  App/Host -> Bootloader  request OTA
```

`0x201` 控制帧 byte0 为 `0xA5` 时，App 先发送 `0x202` ACK，再复位
进入常驻 Bootloader。Linux OTA Host 随后以 `0x300/A5` 完成
Bootloader 握手，所以正常升级无需手动复位。

## 构建

在 Keil 中打开 `Projects/MDK-ARM/atk_f103.uvprojx`，构建
`DHT11_CAN_APP` 并生成 `.bin`。随后用仓库打包器生成 `.ota`，不要把
裸 `.bin` 直接交给 Linux OTA Host。
