# Legacy reference

本目录保存旧 GNCZMKN 的冻结 Git archive。它只用于行为提取、科学对照、资产 provenance 和历史审计。

## 文件

- `legacy-source.zip`：Git HEAD 的完整 tracked source archive。
- `legacy-source.sha256`：归档校验值。
- `source-manifest.json`：来源、commit、范围和测试状态。
- `source-index.md`：蓝图引用的关键源码与测试索引。
- `baseline-test-report.md`：导入时的测试事实。
- `reproduction/`：R0 干净构建、测试、代表任务、输出哈希和工具链兼容性证据。

## 校验与解包

```powershell
Get-FileHash reference/legacy/legacy-source.zip -Algorithm SHA256
powershell -NoProfile -ExecutionPolicy Bypass -File tools/extract-legacy-reference.ps1
```

解包内容进入 `reference/legacy/extracted/`，该目录已被忽略。不得把解包目录加入 CMake、include path 或运行时资源搜索路径。

完整受控复现、固定依赖哈希与结果见 [reproduction/README.md](reproduction/README.md)。常规仓库检查只验证已晋升证据；耗时的 Legacy 全量构建需要显式运行 `tools/reproduce-legacy.ps1`。

## 使用规则

- 可以阅读和执行，用于 evidence capture。
- 可以迁入经过 provenance、oracle 和新 identity 审核的科学内容。
- 不能链接旧 runtime、复制旧公共 API 或建立长期兼容 facade。
- 任何传播行为受 `LICENSE-STATUS.md` 和 D-012 约束。
