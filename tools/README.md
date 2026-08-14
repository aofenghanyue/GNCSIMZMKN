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
- `yyz_6dof_core_reference.py`：以 60 位 CPython `decimal` 独立计算已接受的 fixture-local YYZ 刚体公式与解析轨迹，并验证 C++17 probe 的 intermediates、RK4 收敛、ExactGrid 终止、candidate 丢弃和输入域失败路径。
- `yyz_force_moment_closure_reference.py`：以 60 位 CPython `decimal` 独立计算已接受的 fixture-local `FrozenInterval` 体轴力/矩闭合与解析短轨迹，并验证 C++17 probe 的逐来源力矩搬移、重力分离、贡献顺序等价、输入域失败和物理 mutation。
- `yyz_air_data_kinematics_reference.py`：以 80 位 CPython `decimal` 独立计算已接受的 fixture-local supplied air-data 风速相减、被动四元数旋转、alpha/beta、动压与 Mach，并验证 C++17 probe 的同边界身份、等价性、严格输入域和四条 sign/direction/clamp mutation。
- `yyz_aero_dimensionalization_reference.py`：以 80 位 CPython `decimal` 独立计算已接受的 fixture-local supplied aerodynamic coefficients 到体轴力/矩、aerodynamic reference point 与质心 wrench 的映射，并验证 C++17 probe 的尺度等价性、严格输入域和三条 sign/scale/reference-vector mutation。
- `yyz_uniform_environment_reference.py`：以 80 位 CPython `decimal` 独立计算已接受的 fixture-local supplied uniform environment pure query，并验证 C++17 probe 的 position/tick invariance、air-data/rigid-core consumer link、严格输入域和 Legacy-style altitude decay 失败路径。
- `legacy_sync_commit_reference.py`：核对冻结 Legacy 来源与已通过测试证据，以 `decimal` 推导同步候选结果，并交叉验证独立 C++17 candidate/commit journal 和 early-commit 失败用例。
- `legacy_publish_reference.py`：核对冻结 Legacy 来源与已通过测试证据，以 `decimal` 推导 publish 边界轨迹，并交叉验证独立 C++17 状态投影、双跑确定性和 committed-state mutation 失败用例。
- `legacy_phase_reference.py`：核对冻结 Legacy 来源、环境、capture harness 和两份原始 trace，交叉验证独立 C++17 七阶段调度、双跑确定性、phase swap 与 duplicate 失败用例。
- `legacy_continuous_group_reference.py`：核对冻结 Legacy group/RK4 来源、环境、capture harness 和两份 stage trace，以 50 位 `decimal` 与独立 C++17 joint-state RK4 验证共享候选、组级提交、split closure、membership 失败用例和按 member identity 重排等价性。
- `legacy_csv_reference.py`：核对冻结 Legacy logger/sink 来源、capture harness 和两份 CSV，以 50 位 `decimal`、fixture-local 语义字段映射与独立 C++17 probe 验证 `t_k` 行、发布态字段、四条编码等价路径和六条直接失败路径；重复未映射表头不会改变语义 dataset。
- `legacy_stop_reference.py`：核对冻结 Legacy 终止、logger/sink 来源、capture harness、两份 t0 dataset 与事件 trace，以 `decimal` 和独立 C++17 timeline 验证停止状态先记录、同边界终止、record-field 换序等价、五条独立失败路径及 free-text reason 语义无关性。
- `legacy_simflow_reference.py`：核对冻结 Legacy SimFlow 来源、环境、运行测试、capture harness、两份 effective mission 和四份 dataset，独立完成按字段 identity 的 matrix row 物化与 dataset 映射，并与 C++17 typed probe 交叉验证 effective-mission JSON 重编码、requested-input 声明、case-source 列和 dataset 列重排、普通 `--config` 重放、独立全新工作目录、case 目录外输入和五条直接失败路径。
- `validate-license-provenance.ps1`：校验 Proposed 许可证策略、provenance inventory、Legacy archive 扫描、外部工具身份、故障注入与审计报告。
- `validate-team-toolchain.ps1`：校验角色双人复核、科学/架构独立性、候选工具链矩阵、preset/CI 固定配置、治理阻塞与故障注入。
- `extract-legacy-reference.ps1`：校验并解包只读 legacy archive 到忽略目录。

脚本只操作本仓库中的明确路径。任何生成的研究数据需要经过 fixture/Artifact 晋升流程才能进入版本控制。

Windows 自检入口：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/bootstrap.ps1
```

安装 PowerShell 7 的平台也可以将命令中的 `powershell` 换成 `pwsh`。
