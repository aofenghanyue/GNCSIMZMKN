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
| `r0-schema-contract-lock.json` | Technical lock | 锁定上述三个 v1 contract 的 identity、bytes、field graph、locator 与 consumer policy |

R0 contract instance 都携带 `schema_version`。fixture 与 oracle manifest 必须给出稳定 identity、provenance、expected facts 和 tolerance policy。`PlanProofRecord` 使用七类 proof kind，并把延期能力统一表示为 `DeferredUnsupported`。

ADR-0004 直接记录 v1 identity、field graph、evidence locator 和 PlanProofRecord 边界。`r0-schema-contract-lock.json` 与 schema validator 保护这些技术约束，不依赖机器角色签署或任务验收收据。

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

契约级语义检查补充实际 manifest 中 fixture、expected-fact、oracle-set 与 oracle identity 的跨文件唯一性，并用 mutation 覆盖未来 proof registry 的重复 identity。fixture `authority` 必须解析到当前 role registry，`open_tasks` 必须解析到尚未 `done` 的 backlog task。进入 `executable`/`qualified` 的 fixture facts 与 oracle 必须使用仓库根相对、正斜杠、精确大小写的 evidence locator，并闭合到 stage-0 tracked、非空 regular Git blob 与非空 regular worktree file。`source_refs` 继续作为 opaque provenance locator，当前验证器不把它解释为本地文件路径。

`PlanProofRecord` v1 的 `premises` 只接受扁平 object-map scalar snapshot。typed prerequisite graph、cycle closure、`PlanProofIndex` 与公共 query contract 进入 v2/R2 consumer 设计。三个 v1 schema 只供 repository validation、fixture/oracle authoring、test evidence 和 governance review；`framework/`、`packages/`、`adapters/`、`apps/` 与 `user/` 消费这些 identity 会触发 guard。

Mission Source、Canonical IR、ExecutionPlanDescriptor、DiagnosticRecord、ObservationBatch、RunManifest 和 Artifact schemas 将由对应 R0/R2/R4 任务冻结。蓝图中的 Markdown 示例不能直接冒充 stable schema。

Schema 变更要求：

- 发布新的 schema identity 与 instance version；
- 提供 v1→v2 migration；
- 提供 valid/invalid fixtures；
- 更新 consumer；
- 通过 `tools/verify-repository.ps1`；
- 公共语义变化关联 ADR。
