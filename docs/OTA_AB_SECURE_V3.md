# STM32F103 A/B + SHA-256/ECDSA + 断点续传 CAN OTA v3

本目录是从 `Github/1.0/imx6ull-t113-distributed-iot-gateway` 当前工作树复制出的独立
2.1 开发版。1.0 的源码、暂存区和验收证据未被修改。

## 已实现的边界

- A/B 双 App 槽位，升级只擦除非确认槽。
- Bootloader 在擦除前验证包头 CRC32、Key ID 和 ECDSA P-256 签名。
- ECDSA 签名覆盖硬件 ID、版本、策略、分块大小、A/B 镜像长度、CRC32、完整
  镜像 SHA-256 和两份分块哈希表的 SHA-256。
- 写入后从 STM32 Flash 重算 CRC32 和 SHA-256。
- 候选镜像只允许一次 TRIAL；App 未确认就复位时，Bootloader 自动切回确认槽。
- App 心跳携带槽位 ID，Host 只接受版本和目标槽位都匹配的确认心跳。
- 两个 2KB Metadata 页面组成追加式日志；先写记录正文，最后写 Magic 提交。
- 另有两个 2KB Resume 页面保存包身份、目标槽和已完成页位图；每完成并复核一个
  2KB Flash 页才追加一条记录。
- `.ota3` 携带每页 SHA-256 表；哈希表摘要被 ECDSA 签名覆盖。重连时 Bootloader
  会复核位图对应的 Flash 页，从第一个缺失或损坏页继续。
- 可以一次性导入 v1 已确认的单槽 Metadata，把旧 App 作为过渡期 Slot A。

这实现的是“带发布者签名的 CAN OTA”。它还不是完整安全启动：公钥仍位于普通
Bootloader Flash，未配置 STM32 读写保护，启动时也不重新验证完整 ECDSA 清单。
版本拒绝仍是策略保护，不是受保护单调计数器，因此不能称为密码学防回滚。

## Flash 布局

| 区域 | 起始地址 | 大小 |
|---|---:|---:|
| Bootloader | `0x08000000` | 64KB |
| Slot A | `0x08010000` | 220KB |
| Slot B | `0x08047000` | 220KB |
| Boot Metadata page 0 | `0x0807E000` | 2KB |
| Boot Metadata page 1 | `0x0807E800` | 2KB |
| Resume journal page 0 | `0x0807F000` | 2KB |
| Resume journal page 1 | `0x0807F800` | 2KB |

Slot A 与 Slot B 必须分别链接。不能把链接到 `0x08010000` 的同一个 `.bin` 同时
写入 B 槽。

## 工程

| 组件 | 路径 |
|---|---|
| Secure Bootloader | `stm32/can_ota_bootloader_ab_secure` |
| Slot A App | `stm32/dht11_can_app_slot_a` |
| Slot B App | `stm32/dht11_can_app_slot_b` |
| A/B Metadata | `stm32/common_ab_secure` |
| Linux Host | `linux/can_ota_host_ab_secure` |
| 双镜像签名打包器 | `tools/package_stm32_ota_ab.py` |
| 密钥生成器 | `tools/generate_ota_signing_key.py` |

## 1. 生成签名密钥

正式私钥应在离线机器生成并加密保存，不能部署到 i.MX6ULL、STM32 或发布包。
仓库中的 `ota_trusted_key.c` 是故意无效的占位文件；不替换时 Bootloader 会拒绝
所有包。

```sh
export OTA_KEY_PASSWORD='使用你自己的强口令'
python3 tools/generate_ota_signing_key.py \
  --private-key /secure/offline/stm32-ota-p256-private.pem \
  --public-c stm32/common_ab_secure/ota_trusted_key.c \
  --password-env OTA_KEY_PASSWORD \
  --replace-placeholder
```

仅实验室临时联调才使用 `--development-unencrypted`。私钥路径应位于仓库外；
仓库也已忽略 `secrets/`。

## 2. 构建三个 Keil 工程

按以下顺序打开并构建：

1. `stm32/can_ota_bootloader_ab_secure/Projects/MDK-ARM/atk_f103.uvprojx`
2. `stm32/dht11_can_app_slot_a/Projects/MDK-ARM/atk_f103.uvprojx`
3. `stm32/dht11_can_app_slot_b/Projects/MDK-ARM/atk_f103.uvprojx`

首次迁移必须通过 SWD 烧写新的 Bootloader。两个 App 当前代码版本都是
`2.1.0.0`。

## 3. 生成 `.ota3`

```sh
export OTA_KEY_PASSWORD='使用你自己的强口令'
python3 tools/package_stm32_ota_ab.py \
  --slot-a stm32/dht11_can_app_slot_a/Output/stm32_dht11_can_app_slot_a.bin \
  --slot-b stm32/dht11_can_app_slot_b/Output/stm32_dht11_can_app_slot_b.bin \
  --private-key /secure/offline/stm32-ota-p256-private.pem \
  --key-password-env OTA_KEY_PASSWORD \
  --version 2.1.0.0 \
  --output dist-ota-dev/stm32-dht11-v2.1.0.0.ota3
```

打包器会拒绝链接地址错误、SP/Reset Handler 错误、超出 220KB、非 P-256 私钥
和未知策略位。

## 4. i.MX6ULL Host

```sh
cd linux/can_ota_host_ab_secure
make CC=arm-linux-gnueabihf-gcc
./run_ota.sh ../../dist-ota-dev/stm32-dht11-v2.1.0.0.ota3
```

正式发布包会把该 Host、`run_ota.sh` 和 `setup_can.sh` 持久化安装到：

```text
/opt/iot-gateway/linux/can_ota_host_ab_secure/
```

生产构建不会提供 Hardware ID 预检查绕过。实板故障注入使用的 `-H` 只有在独立
验收构建显式定义 `CAN_OTA_ACCEPTANCE_TEST` 时才会编译，不能把该宏加入发布包。

Bootloader 在 READY 状态的第 8 字节报告目标槽。Host 先发送 248 字节签名清单，
验签通过后发送目标槽的分块哈希表。Bootloader 用包身份匹配断点记录、复核已完成
页，并在 WRITING 状态中报告续传偏移；Host 的数据帧序号从 0 重新开始，但数据
从该偏移继续。Host 不让单帧跨越 2KB 页边界，并在每页写入、读回和日志提交状态
返回后再发送下一页，避免 Flash 编程期间 CAN FIFO 溢出。最后仍等待版本与槽位
完全匹配的心跳。

## 状态机

```text
confirmed=A
    |
    | verify signature, erase/write/verify B
    v
candidate=B, attempted=0
    |
    | Bootloader appends attempted=1 before jump
    v
trial B
    |                         |
    | App confirms            | reset/power loss before confirmation
    v                         v
confirmed=B              rollback -> confirmed=A
```

传输期间掉电不会改变 active/confirmed 记录，旧槽继续可启动。重新发起同一个
`.ota3` 时，已完成且哈希正确的 2KB 页不会重写；未完成页会先擦除再重传。更换包、
版本、Key ID、目标槽或镜像参数会创建新接收会话并从头擦写非确认槽。Metadata
候选记录写入以后掉电则进入一次试启动或自动回滚。

## 必做实板验收

1. A→B 正常升级、确认、完整断电保持。
2. B→A 正常升级，证明两个链接地址都可运行。
3. TRIAL App 在确认前复位，验证自动回滚且回滚状态能保持。
4. 传输约 30% 断电，重新发送同一 `.ota3`，验证 Host 报告非零页对齐偏移且最终成功。
5. 修改清单任一字节并重算 CRC，验证 ECDSA 拒绝。
6. 修改镜像并重算 CRC/SHA/包头 CRC但不持有私钥，验证 ECDSA 拒绝。
7. 使用另一私钥签名，验证 Key ID/签名拒绝。
8. 两个 Boot Metadata 页面轮转时断电，验证仍能选择最新有效记录。
9. 两个 Resume 页面轮转时断电，验证仍选择最新有效位图并复核 Flash。
10. 修改已完成页后重连，验证从首个哈希不匹配页起重新擦写。

本次发布候选已完成第 1～7 项的核心实板链路，并完成最终 Slot B 冷启动保持；
第 8～10 项仍列为扩展故障注入覆盖。准确结论和原始证据见
`docs/acceptance/v2.1.0-rc.1/RESULT.md`。
