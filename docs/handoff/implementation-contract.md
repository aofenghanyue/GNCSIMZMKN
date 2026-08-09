# 实现与证据契约

## 1. 成熟度

| 标记 | 含义 | 可以声称什么 |
| --- | --- | --- |
| `Concept` | 讨论中的想法 | 只能用于 proposal |
| `Fixture` | 有数据级规范实例 | 可用于设计和测试输入 |
| `Gate` | 有可执行退出条件 | 可用于阶段验收 |
| `Implemented` | 源码和自动测试通过 | 可用于新代码 |
| `Qualified` | 独立 reference、适用域和证据闭合 | 可用于声明范围内研究 |

蓝图中的 `V1` 表达目标范围，不自动获得 `Implemented` 状态。

## 2. 实现最小闭环

每个工作包至少交付：

```text
authoritative input
-> validation / transformation
-> authoritative output
-> success evidence
-> failure evidence
```

模型或运行语义工作包还要声明 owner、time、state transition、commit 和 rollback。Workflow 或外部效果工作包还要声明 resource、receipt、idempotency 和 artifact lineage。

## 3. 公共契约冻结条件

- 有稳定 identity 和 version；
- 有机器可读 schema 或 C++ contract；
- 有至少一个真实成功 fixture；
- 有至少一个关键失败 fixture；
- 有序列化、hash 或兼容规则；
- 有 owner role 和变更策略；
- 有下游真实 consumer；
- 架构与科学评审通过。

## 4. 证据等级

| 层次 | 典型证据 |
| --- | --- |
| 数学性质 | 解析解、恒等式、性质测试 |
| 数值算法 | 收敛阶、残差、独立实现 |
| 模型 kernel | 论文表格、MATLAB/Python 中间量 |
| component contract | input/output/time/reset/failure fixture |
| closure | 力矩平衡、dt convergence、hold/candidate 对照 |
| vertical mission | 轨迹、事件、终态指标和能量/质量不变量 |
| research workflow | lineage、报告和可复现 bundle |

最终 CSV 只能承担纵向证据的一部分。

## 5. 未闭合决策

蓝图允许 C++ 模板、对象布局、arena、codec、hash 实现、线程策略和文件命名由窄 ADR 决定。对应 ADR 未接受时，代码只能进入实验分支，不能成为稳定公共面。
