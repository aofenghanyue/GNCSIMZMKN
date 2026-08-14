# R0 当前执行状态

- 更新日期：2026-08-14
- 当前 gate：`R0`
- 产品状态：可构建空骨架与 R0 独立科学 probe；尚无仿真生产能力
- 当前分支：`codex/r0-governance-reset`
- 分支基线：`origin/main@dfedf27`

## 当前治理

仓库所有者已停止原有多智能体执行，并要求改为单一实现智能体、可执行交付优先的工作方式。2026-08-12 的机器角色授权已撤销；AI 可以实现、测试和提出建议，不能进行最终科学、架构、阶段门或发布批准。

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
- `R0-LEG-002` 的七条 oracle 均已有可执行证据 bundle。`ORACLE-YYZ-PUBLISH-01`、`ORACLE-YYZ-PHASE-02`、`ORACLE-YYZ-SYNC-03`、`ORACLE-YYZ-GROUP-04` 与 `ORACLE-YYZ-CSV-05` 已达到 `executable`；仓库所有者已接受只读 publish、`t_k` truth 刷新、固定宏阶段顺序、candidate barrier、committed-`t_k` 读取、共享 RK candidate、单次 scope commit、唯一 identity-bound membership、CSV `t_k`/published-state 边界及对应 Legacy 实现形状退出的逐事实处置。`ORACLE-YYZ-STOP-06` 与 `ORACLE-SIMFLOW-07` 处于 `capturing`：分别覆盖停止状态先记录/同边界终止，以及预运行任务物化/普通任务重放；两条都包含直接失败用例，处置分类等待确认。
- `R0-SCI-001` 已由仓库所有者接受并完成。
- `R0-SCI-002` 已完成，executable bundle 通过独立解析、RK4 收敛、终止与失败检查。
- 其余 R0 任务保持 `planned`。

## 下一条开发主线

1. 收敛 `ORACLE-YYZ-STOP-06` 的 Preserve/Retire 处置选择；SimFlow 的可执行 capture 已准备好进入后续处置。
2. 继续避免同时展开 gate、性能、YYZ、CAVH 和架构治理。

## 保留与恢复

旧工作没有丢失：

- 原工作分支：`codex/r0-gov-002`
- 原分支上已推送提交保持不动
- 未提交的 staged patch 已保存为 stash，说明包含 `pre-governance-reset staged patch a02807de`

除非仓库所有者明确要求，不合并旧治理分支，不应用该 stash，也不改写 Git 历史。
