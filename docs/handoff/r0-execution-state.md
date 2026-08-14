# R0 当前执行状态

- 更新日期：2026-08-15
- 当前 gate：`R0`
- 产品状态：可构建空骨架与 R0 独立科学 probe；尚无仿真生产能力
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
- C++ 与 Python 交叉执行的科学约定检查。
- minimal 3DoF 高精度解析轨迹、独立 C++17 RK4 probe、收敛、终止与失败检查。
- Legacy 只读快照、复现证据和 provenance 边界。
- Windows/MSVC 与 Ubuntu/GCC 的 CI workflow。

这些资产表达技术基线。它们不代表 R1～R8 已实现，也不赋予 AI 决策权。

## 任务概况

- Bootstrap 四项任务保持 `done`。
- `R0-GOV-001`、`R0-ARCH-001`、`R0-SPEC-001` 保留为技术基线完成项。
- `R0-GOV-002` 回到 `planned`，等待真实权利与分发输入。
- `R0-LEG-001` 已完成；固定归档与工具链的干净复跑保持 27/27 测试、五条 CSV 基线和源码指纹一致。
- `R0-LEG-002` 已完成；七条 oracle 均达到 `executable`。仓库所有者已接受只读 publish、`t_k` truth 刷新、固定宏阶段顺序、candidate barrier、committed-`t_k` 读取、共享 RK candidate、单次 scope commit、唯一 identity-bound membership、CSV `t_k`/published-state 边界、停止状态 Observation 先于 RunOutcome、SimFlow 预运行自包含任务物化与 ordinary compile/run replay，并接受对应 Legacy 实现形状退出的逐事实处置。
- `R0-SCI-001` 已由仓库所有者接受并完成。
- `R0-SCI-002` 已完成，executable bundle 通过独立解析、RK4 收敛、终止与失败检查。
- `R0-SCI-003` 已进入 `in_progress`；仓库所有者已接受 fixture-local 刚体核心、四元数归一化策略、`FrozenInterval` 力/矩闭合、supplied air-data kinematics、supplied aerodynamic coefficient dimensionalization、三线性 coefficient lookup 与严格适用域、supplied uniform environment、supplied propulsion response、标量燃耗下质量更新与 CoM/惯量显式保持，以及 committed-boundary 指标、inclusive AtGrid any-of 终止、高优先级选择、terminal-observation-first 结果封存和 fixture-local source-to-result composition 口径。`REF-YYZ-6DOF-CORE-001` 提供独立公式 intermediates、解析与高精度轨迹、收敛、转动守恒量、ExactGrid 终止及关键失败用例；`REF-YYZ-FORCE-MOMENT-CLOSURE-001` 提供逐来源力矩搬移、规范化闭合、重力分离、闭合到刚体核心的解析短轨迹、输入域拒绝，以及 propulsion 预搬移后重复计矩的回归；`REF-YYZ-AIR-DATA-KINEMATICS-001` 提供风速相减、被动旋转、alpha/beta、动压、Mach 与失败路径；`REF-YYZ-AERO-DIMENSIONALIZATION-001` 提供 `[-C_A,+C_Y,-C_N]` 力映射、展长/弦长分离力矩尺度、显式 aerodynamic reference point 与质心力矩搬移；`REF-YYZ-AERO-LOOKUP-001` 提供 immutable 三轴表、三线性 pure query、闭区间域状态、dimensionalization consumer、query/table 失败和四条 interpolation mutation；`REF-YYZ-UNIFORM-ENVIRONMENT-001` 提供 position/tick-invariant 惯性系重力/风、密度/声速、air-data/rigid-core consumer link 与 Legacy-style altitude decay 判别；`REF-YYZ-PROPULSION-RESPONSE-001` 提供显式体轴推力方向、作用点固有力矩、Closure 单次搬移、正消耗区间积分、Mass candidate、区间分割等价和三条定向 mutation；`REF-YYZ-MASS-PROPERTIES-001` 以既有 accepted invariants 实现 committed projection、CoM 点到作用点几何、完整惯量 consumer 和 candidate 提交可见性；`REF-YYZ-FROZEN-INTERVAL-001` 已将 air-data、三线性 lookup、dimensionalization、推进、当前质量属性、Closure 和刚体核心组合成一步独立解析/RK4 轨迹，覆盖查表域/身份拒绝，并直接拒绝风向相加、nearest-grid 替代、候选质量提前使用和推进力矩重复搬移；`REF-YYZ-SCALAR-BURN-MASS-001` 在独立模型身份下按区间扣减质量、逐项保持 CoM/完整惯量并在 closing tick 提交，覆盖 full-inertia、zero-flow、区间分割、耗尽拒绝和四条语义 mutation；`REF-YYZ-TWO-INTERVAL-MASS-COMMIT-001` 把两段 supplied-force FrozenInterval 与标量燃耗连接为 candidate → atomic commit → next consumer 解析/RK4 轨迹，直接拒绝提前质量可见、下一段使用陈旧质量和质量/刚体非原子提交；`REF-YYZ-RUN-EVALUATION-001` 从三份 committed sample 生成 duration、downrange、mass、speed 与极值指标，覆盖 Complete/Abort、同时触发优先级、终端样本封存、十二条输入拒绝和五条跨边界 mutation；`REF-YYZ-MISSION-COMPOSITION-001` 已把十二个 component identity、两段连续 lookup-composed `FrozenInterval`、两次 rigid/mass 原子提交、committed-boundary evaluation、terminal observation 与 mission result 连接成单一执行入口。tick 1 只读取新提交的刚体状态和质量，重新计算 air-data、aero lookup、dimensionalization、propulsion 与 Closure；第二段产生非零俯仰力矩和完整转动状态，1/2/4/8 子步结果提供四阶自收敛证据。supplied-force 双区间轨迹继续承担独立的时间语义检查。完整 `REF-YYZ-001` 仍待 fixture-local diagnostic、制导控制、canonical assets 与生产容差闭合。
- 其余 R0 任务保持 `planned`。

## 下一条开发主线

1. 继续 `R0-SCI-003` 单一主线，在现有两区间 mission composition 上增加 fixture-local 失败诊断切片：把 stale boundary Closure、非原子 rigid/mass commit 或输入域拒绝收敛为带稳定 code、sample tick、component identity 和数值上下文的 machine-valid diagnostic，并保持产品级 Diagnostic contract、canonical mission/assets 与制导控制范围隔离。
2. 继续保持单一 R0 主线，不展开 gate、性能、CAVH 或架构治理。

## 保留与恢复

旧工作没有丢失：

- 原工作分支：`codex/r0-gov-002`
- 原分支上已推送提交保持不动
- 未提交的 staged patch 已保存为 stash，说明包含 `pre-governance-reset staged patch a02807de`

除非仓库所有者明确要求，不合并旧治理分支，不应用该 stash，也不改写 Git 历史。
