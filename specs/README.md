# Machine-readable specifications

本目录保存能够自动验证的仓库、fixture、oracle 和目标契约 schema。

## 当前 schema

| 文件 | 成熟度 | 用途 |
| --- | --- | --- |
| `project-manifest.schema.json` | Implemented for bootstrap | 仓库身份、gate 和模块清单 |
| `task-backlog.schema.json` | Implemented for bootstrap | 工作包格式 |
| `fixture-manifest.schema.json` | Fixture | reference fixture 包 |
| `oracle-manifest.schema.json` | Fixture | 独立 oracle 与容差 |
| `plan-proof-record.schema.json` | Fixture | Compiler proof 参考结构 |

Mission Source、Canonical IR、ExecutionPlanDescriptor、DiagnosticRecord、ObservationBatch、RunManifest 和 Artifact schemas 将由对应 R0/R2/R4 任务冻结。蓝图中的 Markdown 示例不能直接冒充 stable schema。

Schema 变更要求：

- 更新 `$id` 或显式兼容规则；
- 提供 valid/invalid fixtures；
- 更新 consumer；
- 通过 `tools/verify-repository.ps1`；
- 公共语义变化关联 ADR。
