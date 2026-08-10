# ADR-0004: R0 JSON Schema contracts and dependency-free validation

- Status: Proposed
- Date: 2026-08-10
- Owners: Architecture Lead, Validation Lead
- Related tasks: R0-SPEC-001
- Architecture references: 00A §3.4、05 §10.6、11 §5

## Context

R0 需要在科学与 legacy bundle 扩展前冻结 fixture、oracle 和 `PlanProofRecord` 的机器契约。Bootstrap schema 只约束少量顶层字段，仓库检查也只证明 JSON 可以解析，无法在 identity、provenance、tolerance 或 expected facts 缺失时提前失败。

仓库当前以 C++17、CMake 和 PowerShell 为已接受工具链。许可证策略与通用 JSON/YAML 依赖仍未决定，因此本阶段不引入生产依赖或第三方 schema validator。

## Decision

R0 契约采用 JSON Schema draft 2020-12 文档，并在每个 instance 中要求稳定的 `schema_version`。fixture 与 oracle schema 显式要求 provenance、expected facts 和 tolerance policy；proof schema 使用蓝图定义的七类 `proof_kind`，并统一结果为 `Proven | Rejected | DeferredUnsupported`。

仓库提供 PowerShell 5.1/7 兼容的验证工具，执行本阶段 schema 使用到的确定子集。验证器遇到未知 schema 关键字时失败。原始 schema/instance JSON 先经过严格读取，拒绝解码后重复的 object key 与非标准 `NaN`、`Infinity`、`-Infinity` token，再交给同一 schema path 验证。

正例、带预期 diagnostic 的反例和实际 manifests 使用同一命令验证；跨实际 manifests 的稳定 ID 唯一性由契约级语义检查补充。fixture authority 必须解析到 role registry，`open_tasks` 只允许解析到未完成 backlog task。`executable`/`qualified` fixture/oracle 的已有 artifact/evidence 字段必须解析到仓库文件；这只是 R0 evidence completeness check，不冻结未来公共 reference URI grammar。

这些 schema 的成熟度为 `Fixture`。R2 可以在真实 Compiler consumer 和 compatibility evidence 完成后通过后续 ADR 晋升或发布新的 schema version。

## Consequences

- Positive: R0 bundle 在进入科学实现前即可获得确定、跨平台、无网络的失败门禁。
- Positive: schema、examples、实际 manifests 和 CTest 使用同一验证路径。
- Costs: 仓库内验证器只覆盖已声明子集，新增关键字需要同步实现和反例。
- Risks: 它不替代通用 JSON Schema 实现；R2 晋升前需要与独立 validator 做交叉验证。
- Modules kept unchanged: `framework/`、`packages/`、`adapters/`、`user/`、`reference/legacy/`。

## Alternatives considered

- Python `jsonschema`：本机可用，CI 与许可/版本策略尚未冻结，会提前引入工具依赖。
- Node/Ajv：需要新增 package lock、网络安装和第三方许可决定。
- 只做手写字段检查：无法证明 schema 文档与实例验证使用同一约束。

## Verification

- 3 份 schema 共有 6 个 valid 与 16 个带预期 diagnostic 的 invalid examples；
- 5 份实际 fixture/oracle manifests、25 个实际 manifest stable identities 全部通过；
- 9 个 validator failure cases 覆盖 unsupported keyword、损坏/null/array-root JSON、原始/escaped duplicate key 与 `NaN`/`Infinity`/`-Infinity`；
- 5 个 identity mutations 覆盖 fixture、expected fact、oracle set、oracle 与 proof 重复；
- CTest 与 `tools/verify-repository.ps1` 调用 schema conformance suite；
- `tools/bootstrap.ps1` 在 Windows Debug/Release 通过；当前 commit 的 PowerShell 7/hosted CI 证据在 push 前保持 pending。

## Supersession rule

许可证/工具链决定允许采用经过版本锁定的通用 validator，或 R2 Compiler 需要当前子集无法表达的公共契约时，可以提交新 ADR 和 schema major/minor version，并提供双 validator conformance 与迁移 fixture。
