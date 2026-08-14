# Architecture baselines

本目录保存 R0 可机器检查的架构归属与派生基线。

- `authority-registry.json`：记录共享 symbol 的语义权威位置、物理 owner 和 Legacy 名称的首要迁移 owner；
- `architecture-baseline.json`：由术语表、ADR-0003、CMake 和归属索引确定性生成；
- `../quality/terminology-conformance-report.json`：同一次生成得到的检查摘要。

语义定义以 `reference-glossary.md` 及其引用分册为权威，依赖方向以 ADR-0003 为权威。`packages_user` 与 `composition_root` 是逻辑规则标签，ADR-0003 的物理模块保持九项。派生 JSON 只服务自动检查，不进入 runtime。

更新权威输入后执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validate-architecture-baseline.ps1 -Update
```

普通检查不改文件：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validate-architecture-baseline.ps1
```
