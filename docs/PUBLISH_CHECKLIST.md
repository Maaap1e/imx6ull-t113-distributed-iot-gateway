# GitHub 发布检查清单

## 发布前

- 确认根目录 `LICENSE` 和 `README.md` 中的作者信息为 `Maaap1e`。
- 复核 `THIRD_PARTY_NOTICES.md`，不要删除第三方源文件中的版权与许可证声明。
- 只添加自己拍摄的硬件照片、自己制作的架构图或已获得再分发许可的媒体资源。
- 不提交天气 API Key、Wi-Fi 密码、私钥、真实公网地址和个人目录路径。
- 在 i.MX6ULL、T113 和 STM32 实板上重新编译并执行一次完整链路验证。
- 将测试通过的 `.bin`/`.hex` 放入 GitHub Release，不要提交到 Git 历史。

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

## 推荐的首个 Release

```text
Tag: v0.1.0
Title: Three-node prototype with CAN OTA
Assets:
- stm32_can_ota_bootloader.hex
- stm32_dht11_can_app.bin
```

Release 说明建议记录实测板卡、CAN 波特率、TCP 端口、固件链接地址、
CRC32 结果以及当前未实现的签名/回滚能力。
