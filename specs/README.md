# Machine-readable specifications

本目录保存能够自动验证的仓库、fixture、oracle 和目标契约 schema。

## 当前 schema

| 文件 | 成熟度 | 用途 |
| --- | --- | --- |
| `project-manifest.schema.json` | Implemented for bootstrap | 仓库身份、gate 和模块清单 |
| `task-backlog.schema.json` | Implemented for bootstrap | 工作包格式 |
| `fixture-manifest.schema.json` | R0 Fixture | identity、provenance、expected facts 与 oracle 引用 |
| `oracle-manifest.schema.json` | R0 Fixture | 独立 oracle、expected facts 与容差 |
| `plan-proof-record.schema.json` | R0 Fixture | 七类 Compiler proof 参考结构与结果约束 |

Mission Source、Canonical IR、ExecutionPlanDescriptor、DiagnosticRecord、ObservationBatch、RunManifest 和 Artifact schemas 将由对应 R0/R2/R4 任务冻结。蓝图中的 Markdown 示例不能直接冒充 stable schema。

R0 三类 evidence schema 使用 Draft 2020-12、稳定 URN `$id` 和显式 `/1` instance version。任务专属 conformance 命令为：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/verify-r0-spec-001.ps1
```

该命令校验正负样例、跨文档引用、现存 manifest 分类和 schema keyword 白名单。`placeholder/0` 表示 gate blocker，不能通过 `/1` schema。

Schema 变更要求：

- 更新 `$id` 或显式兼容规则；
- 提供 valid/invalid fixtures；
- 更新 consumer；
- 通过 `tools/verify-repository.ps1`；
- 公共语义变化关联 ADR。
