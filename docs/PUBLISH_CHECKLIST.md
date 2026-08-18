# GitHub 发布检查清单

## 发布前

- 确认根目录 `LICENSE` 和 `README.md` 中的作者信息为 `Maaap1e`。
- 复核 `THIRD_PARTY_NOTICES.md`，不要删除第三方源文件中的版权与许可证声明。
- 只添加自己拍摄的硬件照片、自己制作的架构图或已获得再分发许可的媒体资源。
- 不提交天气 API Key、Wi-Fi 密码、私钥、真实公网地址和个人目录路径。
- 在 i.MX6ULL、T113 和 STM32 实板上重新编译并执行一次完整链路验证。
- 完成 `docs/V1_ACCEPTANCE.md`，保留 24 小时资源记录与故障恢复结果。
- 确认 `VERSION`、README 徽章、Release tag 和固件版本相互一致。
- 将测试通过的 `.bin`/`.hex` 放入 GitHub Release，不要提交到 Git 历史。
- 确认私钥、私钥口令、`dist-ota-dev/` 和故障注入 `.ota3` 均未进入源码或运行包。
- 确认生产 Host 未定义 `CAN_OTA_ACCEPTANCE_TEST`，`-H` 测试入口不可用。
- 核对 `docs/acceptance/<version>/EVIDENCE_SHA256SUMS.txt` 后再生成发布资产。
- 执行 `sh scripts/package_acceptance.sh`，保存证据压缩包及其 `.sha256`。

## 初始化仓库

```sh
git init
git add .
git status
git commit -m "Initial public release"
git branch -M main
git remote add origin https://github.com/<username>/<repository>.git
git push -u origin main
```

执行 `git add .` 后先检查 `git status`。预期不应出现 `Output/`、`build/`、
固件二进制、日志、CSV、PID、`.env` 或 T113 私有媒体资源。

## v1.0 发布顺序

```text
Pre-release tag: v1.0.0-rc.1
Pre-release title: v1.0 runtime hardening release candidate

Final tag (only after board acceptance): v1.0.0
Final title: Distributed IoT gateway v1.0
Assets:
- iot-gateway-1.0.0-imx6ull.tar.gz
- iot-gateway-1.0.0-t113.tar.gz
- stm32_can_ota_bootloader.hex
- stm32_dht11_can_app.bin
- acceptance-results.txt
```

Release 说明建议记录实测板卡、CAN 波特率、TCP 端口、固件链接地址、
CRC32 结果以及当前未实现的签名/回滚能力。

## v2.1.0 正式版发布资产

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

T113/LVGL 在本次 OTA 正式版中没有运行时变更，可以继续使用 `v1.1.0` 已验收的
T113 部署；如希望统一版本号，可额外构建并附加
`iot-gateway-2.1.0-t113.tar.gz` 及其 `.sha256`。

`v2.1.0` 直接采用 `v2.1.0-rc.1` 已验收的 STM32 二进制和正常签名 `.ota3` 内容，
不得重新修改或重新签名后仍声称使用原验收证据。正式资产清单使用稳定版文件名，
并记录其 SHA-256 与 RC 资产的对应关系。

公开发布只附加正常签名固件，不附加私钥、口令、错误签名/错误硬件/授权降级等
故障注入包。Release Notes 保留准确的能力边界，但这些量产级扩展不阻塞本项目正式
版本发布。

正式版证据包沿用候选版已经发布和校验的原始实板记录。需要单独重新生成时可执行：

```sh
sh scripts/package_acceptance.sh
```

`package_acceptance.sh` 会读取 `docs/acceptance/v2.1.0/EVIDENCE_SOURCE_VERSION`。
正式 Release 的统一收集脚本则直接复制已经发布的 RC 验收压缩包，保留包内原始
目录和字节，只生成稳定版附件名及外层 SHA-256。

## v2.1.0 正式版发布顺序

1. 确认 `VERSION`、README 徽章、CHANGELOG 和正式版发布说明均为 `2.1.0`。
2. 执行完整自动化测试并确认三个 Keil 工程仍使用已验收配置。
3. 生成 `iot-gateway-2.1.0-imx6ull.tar.gz`、正式证据包和所有 SHA-256 清单。
4. 通过 PR 将纯发布文档与元数据变更合并到 `main`。
5. 在合并后的 `main` 创建新标签 `v2.1.0`；保留且不移动 `v2.1.0-rc.1`。
6. 创建名称为 `v2.1.0 — A/B Secure CAN OTA` 的 GitHub Release。
7. 不勾选 Pre-release，勾选 Set as the latest release，并上传上述正式版资产。
8. 下载 Release 附件后再次执行 SHA-256 校验，确认 GitHub 附件与本地发布目录一致。

Ubuntu 中生成运行包后，可以用下面一条命令收集全部 9 个正式附件：

```sh
sh scripts/collect_v2_1_release_assets.sh /path/to/v2.1.0-rc.1-assets
```

输出目录为 `dist/v2.1.0-release/`。STM32 固件直接复制已验收的 RC 资产，脚本只生成
稳定版文件清单，不重新构建或重新签名固件。
