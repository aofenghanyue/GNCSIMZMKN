# R0-SPEC-001｜R0 机器契约与一致性验证

- 状态：Review
- Assignee：Codex
- Owner role：Architecture Lead
- 目标评审日期：2026-08-10
- 关联 gate：G0 / G1

## 权威输入

- `docs/tasks/backlog.json` 中的 `R0-SPEC-001`；
- `00A §3.4` 的 YYZ `PlanProofRecord` fixture；
- `05 §10.6` 的 proof record/index 语义与结果约束；
- `11 §5` 的 R0 schema、fixture、oracle 和 conformance 退出条件；
- 现有 fixture/oracle manifests 与 `specs/*.schema.json` bootstrap 占位契约。

## 权威输出

1. 带实例版本、严格字段约束和成熟度声明的 fixture、oracle、`PlanProofRecord` JSON Schema；
2. 每份 schema 的有效示例和针对关键缺失字段的无效示例；
3. 可在 Windows PowerShell 5.1 与 PowerShell 7 上运行的仓库内验证命令；
4. CTest 与 repository verification 中可追踪的 schema conformance 证据；
5. 对 schema 版本、验证子集和升级边界的 ADR。

## 失败路径

- schema 或 instance 不是合法 JSON 时失败并定位文件；
- schema 使用验证器未支持的关键字时失败，避免静默跳过约束；
- 标记为 valid 的示例未通过时失败；
- 标记为 invalid 的示例被接受时失败；
- 仓库中的实际 fixture/oracle manifest 不满足 schema 时失败；
- identity、provenance、tolerance policy 或 expected facts 缺失时失败；
- oracle/expected-fact 等稳定 identity 重复时失败；
- `Rejected` 或 `DeferredUnsupported` proof 缺少 diagnostic，或 unsupported proof 暴露可调用 operator 时失败。

## 验收与证据

- `tools/validate-r0-specs.ps1` 对全部实际 manifests 和示例返回成功；
- 每份 schema 至少有一个 valid 与一个 invalid conformance case；
- CTest 包含独立 schema conformance test；
- `tools/bootstrap.ps1` 全量通过；
- `git diff --check`、变更审查清单和最终 commit hash 作为代码评审证据。

## 保持零修改

- `framework/`、`packages/`、`adapters/`、`user/` 与 `reference/legacy/`；
- R1–R8 产品契约和运行能力；
- C++ runtime、Compiler、Session、Artifact Store 与前端路径。

## 升级触发器

- 引入第三方 JSON Schema 实现或新的运行时依赖；
- 修改公共 schema 的 identity/version 兼容规则；
- 把本阶段 fixture 契约直接声明为 R2 Compiler 的 `Implemented` 公共 API。

## 评审记录

日期：2026-08-10。

- contract review：fixture/oracle/proof 分别承担 reference bundle、独立判据和 Compiler assertion，未形成第二套 runtime contract；
- architecture review：变更停留在 `specs/`、fixtures、oracles 与 repository tools，`framework/`、Kernel、packages、adapters、user 和 legacy archive 保持零修改；
- failure review：缺 provenance/tolerance/expected facts、重复 identity、Rejected 无 diagnostic、DeferredUnsupported 暴露 operator、未知 schema keyword 和损坏 JSON 均由自动反例覆盖；
- review findings：修正 backlog 中过时的 proof 章节引用，并为 PowerShell 将 JSON `null` 表示为 `$null` 的根值路径增加显式类型校验与回归例；
- verification：3/3 CTest、repository verification、`git diff --check` 与 Python `jsonschema` Draft 2020-12 交叉检查通过；
- residual risk：仓库验证器有意限制为 R0 使用子集；ADR-0004 在 owner role 指派前保持 `Proposed`，任务因此停留在 `review`。
