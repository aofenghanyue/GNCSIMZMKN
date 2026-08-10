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

R0 contract instance 都携带 `schema_version`。fixture 与 oracle manifest 必须给出稳定 identity、provenance、expected facts 和 tolerance policy。`PlanProofRecord` 使用七类 proof kind，并把延期能力统一表示为 `DeferredUnsupported`。

## 一致性验证

默认命令验证三份 schema、仓库内实际 manifests、valid examples、带预期 diagnostic 的 invalid examples、严格 JSON 失败路径和跨文件 identity mutations：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validate-r0-specs.ps1
```

也可以验证单个 instance：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validate-r0-specs.ps1 `
  -SchemaPath specs/plan-proof-record.schema.json `
  -InstancePath specs/examples/plan-proof-record/valid/yyz-guidance-rate.json
```

`tools/modules/JsonSchemaSubset.psm1` 实现 R0 schema 已使用的确定子集。schema 出现未支持关键字会直接失败；新增关键字需要同时加入 valid/invalid conformance case。原始 JSON 在 `ConvertFrom-Json` 前经过严格词法/语法读取：解码后重复的 object key、`NaN`、`Infinity` 和 `-Infinity` 都会失败。

契约级语义检查补充实际 manifest 中 fixture、expected-fact、oracle-set 与 oracle identity 的跨文件唯一性，并用 mutation 覆盖未来 proof registry 的重复 identity。fixture `authority` 必须解析到当前 role registry，`open_tasks` 必须解析到尚未 `done` 的 backlog task。进入 `executable`/`qualified` 的 fixture facts 与 oracle 必须把证据引用闭合到仓库文件；当前检查仅把 manifest-relative 或 repository-relative 字符串作为仓库证据定位方式，不裁决未来公共 URI/reference grammar。

Mission Source、Canonical IR、ExecutionPlanDescriptor、DiagnosticRecord、ObservationBatch、RunManifest 和 Artifact schemas 将由对应 R0/R2/R4 任务冻结。蓝图中的 Markdown 示例不能直接冒充 stable schema。

Schema 变更要求：

- 更新 `$id` 或显式兼容规则；
- 提供 valid/invalid fixtures；
- 更新 consumer；
- 通过 `tools/verify-repository.ps1`；
- 公共语义变化关联 ADR。
