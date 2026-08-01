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

## v2.1.0-rc.1 发布资产

```text
iot-gateway-2.1.0-rc.1-imx6ull.tar.gz
iot-gateway-2.1.0-rc.1-imx6ull.tar.gz.sha256
iot-gateway-2.1.0-rc.1-t113.tar.gz
iot-gateway-2.1.0-rc.1-t113.tar.gz.sha256
stm32_can_ota_ab_secure_bootloader.hex
stm32_dht11_can_app_slot_a-v2.1.0.3.bin
stm32_dht11_can_app_slot_b-v2.1.0.3.bin
stm32-dht11-v2.1.0.3.ota3
v2.1.0-rc.1-stm32-assets.sha256
v2.1.0-rc.1-acceptance-evidence.tar.gz
```

公开发布只附加正常签名固件，不附加私钥、口令、错误签名/错误硬件/授权降级等
故障注入包。Release Notes 必须保留“签名 OTA 不是完整安全启动”和尚未完成的三项
扩展故障注入边界。
