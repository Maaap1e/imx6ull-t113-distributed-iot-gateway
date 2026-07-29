# i.MX6ULL STM32 CAN OTA Host

该工具运行在 i.MX6ULL Linux 上，只接受由仓库打包器生成并校验通过的
`.ota` 固件包。它会在擦除前发送目标硬件 ID、版本和降级策略，然后
传输 App 镜像、输出 Bootloader 状态，并等待 App 确认后发出的匹配
版本心跳。未收到确认心跳时命令返回失败。

发起升级时，Host 先通过 App 控制帧 `0x201/A5` 请求运行中的 App
复位进入 Bootloader，再重复发送 Bootloader ENTER 帧 `0x300/A5`。
设备已经处于恢复模式时也可直接响应后者，因此不需要人工按复位键。

## 编译

```sh
make CC=arm-linux-gnueabihf-gcc
```

## 生成固件包

```sh
python3 tools/package_stm32_ota.py \
  --input stm32/dht11_can_app/Output/stm32_dht11_can_app.bin \
  --output stm32_dht11_can_app-v1.2.0.3.ota \
  --version 1.2.0.3
```

## 执行

```sh
./setup_can.sh can0 500000
./stm32_can_ota_host -i can0 -f stm32_dht11_can_app-v1.2.0.3.ota
```

安装到 `/opt/iot-gateway` 后推荐使用包装脚本。该脚本会暂停 CAN
遥测客户端、持有 OTA 锁、配置 can0，并在结束后恢复原服务：

```sh
./run_ota.sh /tmp/stm32_dht11_can_app-v1.2.0.3.ota
```

可用选项：

```text
-i  CAN 接口，默认 can0
-f  .ota 固件包
-p  每帧间隔，默认 20000 us
-t  状态等待超时，默认 5000 ms
```

协议、Flash 布局、试启动确认和故障注入方法见
[`docs/OTA_FLOW.md`](../../docs/OTA_FLOW.md)。
