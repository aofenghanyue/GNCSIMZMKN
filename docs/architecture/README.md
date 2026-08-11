# Architecture baselines

本目录保存 R0 可机器检查的架构归属与派生基线。

- `authority-registry.json`：唯一记录共享 symbol 的语义权威位置、物理 owner，以及 Legacy 名称的首要迁移 owner；
- `architecture-baseline.json`：由术语表、ADR-0003、CMake 和归属索引确定性生成，不能手工编辑；
- `../quality/terminology-conformance-report.json`：同一次生成得到的检查结果与计数，不能手工编辑。

语义定义继续以 `reference-glossary.md` 及其引用分册为权威，依赖方向继续以 ADR-0003 为权威。派生 JSON 只服务于审查、自动门禁和后续 R0 guard，不进入 runtime。文本源 hash 按 UTF-8、LF、无 BOM 归一化，避免平台换行差异改变语义指纹。

更新权威输入后执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validate-architecture-baseline.ps1 -Update
```

普通检查不改文件：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validate-architecture-baseline.ps1
```
