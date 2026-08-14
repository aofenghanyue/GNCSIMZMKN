# R0 当前执行状态

- 更新日期：2026-08-15
- 当前 gate：`R0`
- 产品状态：可构建空骨架、R0 独立科学/性能 probe 与 executable source-boundary guard；尚无仿真生产能力
- 当前分支：`codex/r0-governance-reset`
- 分支基线：`origin/main@dfedf27`

## 当前治理

仓库所有者已停止原有多智能体执行，并要求改为单一实现智能体、可执行交付优先的工作方式。2026-08-12 的机器角色授权已撤销。仓库所有者已授权当前 R0 范围内的后续推荐科学与架构项按默认接受记录并实施；许可、阶段门、发布和明显扩大范围的选择仍保留给仓库所有者。

当前规则以以下文件为准：

1. `AGENTS.md`
2. `docs/tasks/backlog.json`
3. 本文件
4. 当前任务引用的 accepted ADR 与架构分册

旧的机器互签、任务验收收据、CI 回执副本、readiness 快照和 reconciliation 审计已经从当前文件树移除。它们仍可从 Git 历史恢复，不参与新任务判断。

## 已保留的技术资产

- Bootstrap 构建、CLI self-check 和 CMake presets。
- R0 fixture、oracle 与 PlanProofRecord schema 及其直接正反例。
- 术语、模块依赖和 Legacy ownership 的派生架构基线。
- production C/C++ include、逻辑 source root、唯一 source owner、package/project 边界和 Legacy 源码/CMake 禁入守卫。
- 全量 Git 跟踪文件分发范围、实际 CMake/CI 外部输入、需审查 binary/archive 和 Legacy license-signal 的直接验证器。
- observation-only minimal 3DoF batch benchmark、四个 concrete scale points、fresh-process D1、sanitized hardware/build profile 和 44 条 raw timing samples。
- C++ 与 Python 交叉执行的科学约定检查。
- minimal 3DoF 高精度解析轨迹、独立 C++17 RK4 probe、收敛、终止与失败检查。
- Legacy 只读快照、复现证据和 provenance 边界。
- Windows/MSVC 与 Ubuntu/GCC 的 CI workflow。

这些资产表达技术基线。它们不代表 R1～R8 已实现，也不赋予 AI 决策权。

## 任务概况

- Bootstrap 四项任务保持 `done`。
- `R0-GOV-001`、`R0-ARCH-001`、`R0-SPEC-001` 保留为技术基线完成项。
- `R0-GOV-002` 当前为 `blocked`，没有活动 assignee。全部 Git 跟踪文件都唯一落入 repository content、architecture blueprint 或 Legacy reference 三个真实范围；当前 CMake 只发现已登记的 `Python3` 验证依赖，CI 只发现固定 commit 的 `actions/checkout`，唯一跟踪 binary/archive 为 Legacy ZIP。Legacy 实时扫描未发现 license-named entry 或强许可证文本信号。四条直接失败用例拒绝 owner decision 绕过、Legacy 外发、未登记 binary vendoring 和下载式 CMake 依赖。外部分发仍受仓库所有者的 G1 范围选择、当前 public origin 处置、蓝图权利和 Legacy 权利阻塞。
- `R0-LEG-001` 已完成；固定归档与工具链的干净复跑保持 27/27 测试、五条 CSV 基线和源码指纹一致。
- `R0-LEG-002` 已完成；七条 oracle 均达到 `executable`。仓库所有者已接受只读 publish、`t_k` truth 刷新、固定宏阶段顺序、candidate barrier、committed-`t_k` 读取、共享 RK candidate、单次 scope commit、唯一 identity-bound membership、CSV `t_k`/published-state 边界、停止状态 Observation 先于 RunOutcome、SimFlow 预运行自包含任务物化与 ordinary compile/run replay，并接受对应 Legacy 实现形状退出的逐事实处置。
- `R0-SCI-001` 已由仓库所有者接受并完成。
- `R0-SCI-002` 已完成，executable bundle 通过独立解析、RK4 收敛、终止与失败检查。
- `R0-SCI-003` 已完成；仓库所有者已接受 fixture-local 刚体核心、四元数归一化策略、`FrozenInterval` 力/矩闭合、supplied air-data kinematics、supplied aerodynamic coefficient dimensionalization、三线性 coefficient lookup 与严格适用域、supplied uniform environment、supplied propulsion response、标量燃耗下质量更新与 CoM/惯量显式保持，以及 committed-boundary 指标、inclusive AtGrid any-of 终止、高优先级选择、terminal-observation-first 结果封存和 fixture-local source-to-result composition 口径。`REF-YYZ-6DOF-CORE-001` 提供独立公式 intermediates、解析与高精度轨迹、收敛、转动守恒量、ExactGrid 终止及关键失败用例；`REF-YYZ-FORCE-MOMENT-CLOSURE-001` 提供逐来源力矩搬移、规范化闭合、重力分离、闭合到刚体核心的解析短轨迹、输入域拒绝，以及 propulsion 预搬移后重复计矩的回归；`REF-YYZ-AIR-DATA-KINEMATICS-001` 提供风速相减、被动旋转、alpha/beta、动压、Mach 与失败路径；`REF-YYZ-AERO-DIMENSIONALIZATION-001` 提供 `[-C_A,+C_Y,-C_N]` 力映射、展长/弦长分离力矩尺度、显式 aerodynamic reference point 与质心力矩搬移；`REF-YYZ-AERO-LOOKUP-001` 提供 immutable 三轴表、三线性 pure query、闭区间域状态、dimensionalization consumer、query/table 失败和四条 interpolation mutation；`REF-YYZ-UNIFORM-ENVIRONMENT-001` 提供 position/tick-invariant 惯性系重力/风、密度/声速、air-data/rigid-core consumer link 与 Legacy-style altitude decay 判别；`REF-YYZ-PROPULSION-RESPONSE-001` 提供显式体轴推力方向、作用点固有力矩、Closure 单次搬移、正消耗区间积分、Mass candidate、区间分割等价和三条定向 mutation；`REF-YYZ-MASS-PROPERTIES-001` 以既有 accepted invariants 实现 committed projection、CoM 点到作用点几何、完整惯量 consumer 和 candidate 提交可见性；`REF-YYZ-FROZEN-INTERVAL-001` 已将 air-data、三线性 lookup、dimensionalization、推进、当前质量属性、Closure 和刚体核心组合成一步独立解析/RK4 轨迹，覆盖查表域/身份拒绝，并直接拒绝风向相加、nearest-grid 替代、候选质量提前使用和推进力矩重复搬移；`REF-YYZ-SCALAR-BURN-MASS-001` 在独立模型身份下按区间扣减质量、逐项保持 CoM/完整惯量并在 closing tick 提交，覆盖 full-inertia、zero-flow、区间分割、耗尽拒绝和四条语义 mutation；`REF-YYZ-TWO-INTERVAL-MASS-COMMIT-001` 把两段 supplied-force FrozenInterval 与标量燃耗连接为 candidate → atomic commit → next consumer 解析/RK4 轨迹，直接拒绝提前质量可见、下一段使用陈旧质量和质量/刚体非原子提交；`REF-YYZ-RUN-EVALUATION-001` 从三份 committed sample 生成 duration、downrange、mass、speed 与极值指标，覆盖 Complete/Abort、同时触发优先级、终端样本封存、十二条输入拒绝和五条跨边界 mutation；`REF-YYZ-MISSION-COMPOSITION-001` 把十二个 component identity、两段连续 lookup-composed `FrozenInterval`、两次 rigid/mass 原子提交、committed-boundary evaluation、terminal observation 与 mission result 连接成单一执行入口。tick 1 读取新提交的刚体状态和质量，重新计算 air-data、aero lookup、dimensionalization、propulsion 与 Closure；同一 committed observation 进入 fixture-local altitude PD guidance 和 pitch-moment PD controller，`0.04 rad` 限幅指令经 unit-gain 零延迟理想变换形成 `20 N·m` 的 `+B-y` moment contribution，并只在 `[tick 1,tick 2)` 加入 Closure。第二段受控 full-state RK4 的 1/2/4/8 子步结果保持四阶自收敛；stale observation、反馈符号、限幅绕过、moment 丢失和轴向反转 mutation 均被独立 Python/C++ reference 拒绝。三类 precommit 失败形成 fixture-local 结构化诊断：stale Closure 对应 `GNC-SCH-0201`，rigid/mass 非原子 candidate 对应 `GNC-INT-0301`，超出 Mach 适用域对应 `GNC-PHY-0201`；每类都携带 sample tick、component identity、数值上下文、独立 policy decision 和未发布 candidate 的回滚结果。`REF-YYZ-001` 现已提供 executable R0 canonical source、十二个 component binding、七类选定资产和 1010 个 C++ probe 叶字段的 tolerance/difference report；527 个 exact 字段与 483 个数值字段全部通过，六条 canonical source/asset/profile 负例全部拒绝。00A 的 30 秒、100 Hz target profile 保持 `target_pending`，没有进入当前科学 verdict。
- `R0-SCI-004` 已完成；仓库所有者接受 `MODEL-CAVH-LEGACY-TRANSCRIBED-FORMULA-001` 作为 fixture-local qualification identity。`REF-CAVH-FORMULA-001` 已固定论文 citation metadata 与 source-access boundary、七条科学假设、两个解析抛物线包络案例、指数密度/Mach/`CL_star` 导数及收敛梯、三组 Eq17/Eq18 全中间量案例、四组 TDCT 符号与饱和案例、十一条显式失败和七条 scientific mutation。Eq17 导数退化返回 `derivative-degenerate` 且 fallback 为 `forbidden`；公式分母奇异返回 `formula-singularity`，不再沿用 Legacy 的静默 Eq18 fallback 或 signed denominator clamp。80 位 Decimal 与独立 C++17 probe 在 Debug/Release 交叉通过。论文逐式一致性、digitized aero、closed-loop 性能和产品 guidance contract 保持未声明。
- `R0-ARCH-002` 已完成；`validate-source-boundaries.ps1` 从 ADR-0003 和 authority registry 投影 source policy，扫描 production C/C++ include、runtime Legacy path 与 CMake。当前仓库正向 inventory 通过；同一 evaluator 拒绝重复 source owner、Kernel→Compiler dot-segment include、Adapter→Kernel、package→Compiler、framework→user、未知内部模块、Legacy path/API 和 Legacy CMake 八个反例。既有 architecture baseline 的十五个反例继续覆盖 shared-symbol/Legacy ownership、DAG 与 CMake edge。没有 runtime artifact 的 state/descriptor/transaction 语义保持 awaiting-artifact。
- `R0-PERF-001` 已完成；`PERF-R0-M3DOF-BATCH-001` 在一个独立 C++17 executable 中运行 1、64、1024 和 16384 episodes，每 episode 为 80 个 fixed RK4 steps。80 位 Decimal comparator 先验证解析正确性，三个 fresh process 的 parsed semantic result 达到当前 workload-scoped D1；D2/D3 保持 pending。2026-08-15 的 observation-only baseline 保存两个 warm-up 与九个 measured process/point，共 44 条 raw samples。最大点 `batch-16384` 的本机 median 为 `16,534,400 ns`，p95 为 `17,576,400 ns`，median throughput 约 `79.27 million steps/s`。硬件为 Intel i7-12700K / 20 logical processors / 32 GiB / Hyper-V；当前 binary 为未进入产品支持 profile 的 Windows MinGW `gcc-15.1.0`，结果不构成 performance threshold、产品 toolchain 或 realtime 资格。
- `R0-GOV-002` 保持 `blocked`，`R0-GATE-001` 保持 `planned`；gate 仍受仓库所有者的 G1 分发范围决定阻塞。

## 下一条开发主线

1. 仓库所有者确认 G1 是否采用“内部开发、停止新增外部分发”的推荐范围。接受后记录 ADR-0008 并运行 G0/G1 gate 前的最后直接验证；若继续公开开发，则需要先确定仓库许可证并处置 public origin 中的 Legacy archive 与蓝图权利。
2. 产品 schema、loader、Compiler、Session、durable evidence 与 R1～R8 runtime 能力继续保持锁定，直到许可证输入与 R0 gate 决定闭合。

## 保留与恢复

旧工作没有丢失：

- 原工作分支：`codex/r0-gov-002`
- 原分支上已推送提交保持不动
- 未提交的 staged patch 已保存为 stash，说明包含 `pre-governance-reset staged patch a02807de`

除非仓库所有者明确要求，不合并旧治理分支，不应用该 stash，也不改写 Git 历史。
