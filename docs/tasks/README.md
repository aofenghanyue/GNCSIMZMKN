# 任务系统

`backlog.json` 是唯一机器可读任务源。阶段分册解释目标和 gate，不复制任务状态。

## 状态

| 状态 | 含义 |
| --- | --- |
| `planned` | 已登记，依赖尚未闭合 |
| `ready` | 输入和依赖齐全，可以领取 |
| `in_progress` | 已有 assignee 和活动分支 |
| `blocked` | 缺少外部决定、资料或前置产物 |
| `review` | 实现完成，等待科学/架构/代码评审 |
| `done` | 验收与 evidence 全部通过 |

## 领取规则

1. 任务必须属于当前已解锁 gate。
2. `depends_on` 中的任务全部为 `done`。
3. 填写 `assignee` 和目标 review 日期。
4. 核对 architecture refs、deliverables、acceptance 和 evidence。
5. 创建 issue/PR，并在标题中保留 task ID。

## 拆分规则

任务过大时可以新增子任务，父任务仍保留 gate 责任。子任务 ID 采用 `<parent>-A`、`<parent>-B`。拆分不能改变 owner、权威输出或阶段门；涉及这些变化时需要 ADR。

## 完成规则

`done` 需要：

- 产物存在且可定位；
- 自动测试通过；
- 失败路径有证据；
- 文档与 schema 同步；
- 所需 reviewer 已批准；
- 对应 evidence hash 或 CI run 可追踪。
