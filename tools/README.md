# Repository tools

- `bootstrap.ps1`：配置、构建、运行 CTest 和仓库检查。
- `verify-repository.ps1`：验证目录、JSON、任务依赖、legacy hash、架构边界、UTF-8 和 Markdown 链接。
- `validate-r0-specs.ps1`：验证 R0 fixture、oracle、PlanProofRecord schema、严格 JSON、实际 manifest registry/证据闭包、正反例与 identity mutations。
- `validate-architecture-baseline.ps1`：生成或检查术语、共享 symbol、模块 DAG 和 Legacy 归属派生基线，并运行反例。
- `reproduce-legacy.ps1`：从冻结 ZIP 和固定哈希的离线依赖归档创建隔离工作区，构建并运行 Legacy 全部测试与五条代表任务。
- `validate-legacy-reproduction.ps1`：校验受控 Legacy 证据索引、环境、测试、任务基线和 MSVC 兼容性缺口，不重新执行耗时构建。
- `scientific_conventions_reference.py`：只使用 CPython 标准库计算 R0 单位、frame、时间与被动 Hamilton 四元数参考结果。
- `validate-scientific-conventions.ps1`：校验 scientific convention fixture，运行 C++/Python 交叉验证、失败路径检查并核对科学结果。
- `minimal_3dof_reference.py`：以 CPython `decimal` 闭式解生成 minimal 3DoF oracle，并按语义字段验证独立 C++17 RK4 probe 的轨迹、收敛、终止和失败结果。
- `legacy_sync_commit_reference.py`：核对冻结 Legacy 来源与已通过测试证据，以 `decimal` 推导同步候选结果，并交叉验证独立 C++17 candidate/commit journal 和 early-commit 失败用例。
- `legacy_publish_reference.py`：核对冻结 Legacy 来源与已通过测试证据，以 `decimal` 推导 publish 边界轨迹，并交叉验证独立 C++17 状态投影、双跑确定性和 committed-state mutation 失败用例。
- `validate-license-provenance.ps1`：校验 Proposed 许可证策略、provenance inventory、Legacy archive 扫描、外部工具身份、故障注入与审计报告。
- `validate-team-toolchain.ps1`：校验角色双人复核、科学/架构独立性、候选工具链矩阵、preset/CI 固定配置、治理阻塞与故障注入。
- `extract-legacy-reference.ps1`：校验并解包只读 legacy archive 到忽略目录。

脚本只操作本仓库中的明确路径。任何生成的研究数据需要经过 fixture/Artifact 晋升流程才能进入版本控制。

Windows 自检入口：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/bootstrap.ps1
```

安装 PowerShell 7 的平台也可以将命令中的 `powershell` 换成 `pwsh`。
