# 阶段门

## Gate 总览

| Gate | 阶段 | 核心结果 | 解锁内容 |
| --- | --- | --- | --- |
| B0 | Bootstrap | 构建、测试、文档、任务和 legacy 边界可验证 | R0 |
| G0 | R0 架构闭合 | 术语、变化分流、proof schema、依赖守卫闭合 | R0 科学验收 |
| G1 | R0 科学基线 | minimal 3DoF、YYZ、CAVH oracle bundle 可执行 | R1 |
| G2 | R1 Model Ecosystem | algorithms、recipes、mechanisms 与 packages 可独立测试 | R2 |
| G3 | R2 Plan Firewall | source 可生成完整、可解释、不可变 plan | R3 |
| G4 | R3 事务语义 | normal/failure/cancel/terminal 有唯一提交结果 | R3 纵向切换 |
| G5 | R3 纵向证明 | YYZ、多实体、故障和 activation fixture 通过 | G6 |
| G6 | R3 legacy 独立 | 新 runner 只依赖新 Compiler/Session | R4、R6 |
| G7 | R4 Evidence Firewall | observation、manifest、lineage 与 sink failure 闭合 | R5 |
| G8 | R5 研究闭环 | DATCOM/配平/分析/仿真/报告纵向链通过 | M2 |
| G9 | R6 Experiment/Python | 多 Session、case、reset/step 与 Python 复用同一语义 | R7/R8 |
| G10 | R7/R8 多入口边界 | authoring、control、snapshot 和 backend 无内部捷径 | M3 |

## Gate 决策记录

每次 gate 评审需要在 `docs/quality/gate-decisions/` 新增一份记录，包含：

- 评审范围和 commit；
- 必需证据及 hash；
- 未完成项和 waiver；
- 科学差异分类；
- 性能与确定性结果；
- 批准人；
- `Passed`、`Conditional` 或 `Failed` 结论。

`Conditional` 不能解锁会消费缺失契约的后续阶段。
