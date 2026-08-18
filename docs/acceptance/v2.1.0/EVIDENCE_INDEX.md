# v2.1.0 正式版证据索引

`v2.1.0` 没有改变 `v2.1.0-rc.1` 已验收的固件和运行行为，因此不重复复制原始
记录。正式版使用以下候选版证据作为发布依据：

- [完整验收结果](../v2.1.0-rc.1/RESULT.md)
- [原始证据索引](../v2.1.0-rc.1/EVIDENCE_INDEX.md)
- [原始证据校验清单](../v2.1.0-rc.1/EVIDENCE_SHA256SUMS.txt)
- [原始证据目录](../v2.1.0-rc.1/raw/)

生成正式版证据归档时执行：

```sh
sh scripts/package_acceptance.sh
```

生成的归档名为 `v2.1.0-acceptance-evidence.tar.gz`，内部保留
`v2.1.0-rc.1` 原始目录名，避免对历史证据重新命名或制造新的测试时间线。
