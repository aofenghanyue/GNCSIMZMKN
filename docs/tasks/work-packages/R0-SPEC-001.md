# R0-SPEC-001｜R0 机器契约与一致性验证

- 状态：Done
- Assignee：`r0-architecture-agent`（machine agent，`/root/r0_architecture_agent`）
- Reviewer：`r0-validation-agent`（machine agent，`/root/r0_validation_agent`）
- Owner role：Architecture Lead
- 接受日期：2026-08-12
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
6. 锁定 v1 `$id`、instance version、原始字节与 field graph 的机器契约，以及 executable evidence locator、consumer 和 v2 migration policy。

## 失败路径

- schema 或 instance 不是严格合法 JSON 时失败并定位文件；解码后重复 object key 与 `NaN`、`Infinity`、`-Infinity` 也失败；
- schema 使用验证器未支持的关键字时失败，避免静默跳过约束；
- 标记为 valid 的示例未通过时失败；
- 标记为 invalid 的示例被接受时失败；
- 仓库中的实际 fixture/oracle manifest 不满足 schema 时失败；
- identity、provenance、tolerance policy 或 expected facts 缺失时失败；
- fixture/oracle/expected-fact/proof 等稳定 identity 在实际 registry 或 mutation 中重复时失败；
- fixture authority 不在 role registry、`open_tasks` 不存在或已 `done` 时失败；
- `executable`/`qualified` fixture fact 或 oracle 的 evidence/artifact 文件缺失、越出仓库或为空时失败；
- executable evidence locator 使用 manifest-relative、absolute、drive-relative、file URI、反斜杠、`.`/`..` segment、query、fragment、percent encoding、大小写别名或未跟踪路径时失败；目标不是 stage-0 tracked、非空 regular Git blob 与非空 regular worktree file 时失败；
- `Rejected` 或 `DeferredUnsupported` proof 缺少 diagnostic，或 unsupported proof 暴露可调用 operator 时失败。
- v1 schema `$id`、instance version、raw bytes 或 field graph 漂移，`premises` 注入 typed prerequisite overlay，或 Fixture schema 被产品/runtime 路径消费时失败。

## 验收与证据

- `tools/validate-r0-specs.ps1` 对全部实际 manifests 和示例返回成功；
- `specs/r0-schema-contract-lock.json` 与三份 v1 schema 的 identity、raw bytes 和 object field graph 精确一致；
- 每份 schema 至少有一个 valid 与一个 invalid conformance case，且每个 invalid case 固定预期 diagnostic；
- CTest 包含独立 schema conformance test；
- `tools/bootstrap.ps1` 全量通过；
- `git diff --check`、变更审查清单和最终 commit hash 作为代码评审证据。
- ADR-0004、`RECON-DEC-001`～`003` 与任务验收记录绑定同一累计技术提交、文件集和独立复核结论；
- push 与 pull-request 的 Ubuntu/Windows 固定 profile 对该技术提交全部成功。

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
- failure review：缺 provenance/tolerance/expected facts、未知/已完成 registry reference、缺 executable evidence、跨文件重复 identity、Rejected/DeferredUnsupported 结果约束、未知 schema keyword、重复 JSON key、非有限 token 和损坏 JSON 均由自动反例或 mutation 覆盖；
- review findings：修正 backlog 中过时的 proof 章节引用，并为 PowerShell 将 JSON `null` 表示为 `$null` 的根值路径增加显式类型校验与回归例；
- initial verification：`6613186` 上 3/3 CTest、repository verification、`git diff --check` 与 Python `jsonschema` Draft 2020-12 schema 交叉检查通过；
- reconciliation amendment historical baseline：schema bytes/version/field graph 保持不变；PowerShell 5.1 suite 验证 5 actual manifests、6 valid、16 invalid、9 validator failures、25 actual-manifest identities 和 5 identity mutations；独立 Python `jsonschema` Draft 2020-12 交叉检查接受 6 valid、拒绝 8 schema-level invalid，并按预期接受由 repository semantic layer 拒绝的另 8 例；Windows Debug/Release 各 9/9 CTest、repository verification（56 JSON、65 tasks、98 Markdown）和 `git diff --check` 通过；当时的 PowerShell 7/current hosted evidence 尚未取得；
- owner implementation：2026-08-12 由授权 `r0-architecture-agent` 实施初始技术冻结，`r0-validation-agent` 保留独立最终复核；基线绑定 `611a48a23ea02ecd0c210a2b101f5c5cbf5df0e6`；
- contract lock：三份 v1 schema 原始 bytes 不变；machine lock 固定 `$id`、instance version、raw SHA-256/byte length 与 object field graph，maturity 保持 Fixture，产品/runtime consumer 为零；
- locator review：executable evidence 使用 repository-root-only locator，并闭合到 tracked、非空 regular Git blob 与 worktree file；valid example 证明 `source_refs` 仍是 opaque provenance locator；
- failure review：新增 20 个 lock/locator/typed-overlay/consumer mutation，原有 16 invalid examples、9 parser failures 与 5 identity mutations保持；percent encoding 在 syntax 层直接失败，mutation 固定命中对应 diagnostic；
- invocation review：Git object 查询显式使用 repository root；CTest 从构建目录调用与仓库根直接调用均通过同一 evidence locator 检查；
- owner disposition：`r0-architecture-agent` 对基线 `611a48a23ea02ecd0c210a2b101f5c5cbf5df0e6` 到技术提交 `ee7157359e689114d0259a1ae7884a315b029bc1` 的累计 13 路径文件集给出 `accept-as-written`；文件集摘要为 `c7e6bf8bc99b309d42d6aa823c69ee57064a921c2c429fafd58350ce3674ec47`；
- independent review：`r0-validation-agent` 对同一 Git 对象、schema blob、20 项 mutation、CTest 9/9、repository verification 60 JSON/65 tasks/100 Markdown、严格 UTF-8 与禁用句式扫描给出 `approved`；
- hosted evidence：push run `31565404481` 与 pull-request run `31565406888` 的 Ubuntu/Windows 四个 job 全部成功；PR 临时 merge commit 为 `52a6630394a2c9720971bae677eea9cb2b71a674`，tree 为 `03e49d5b524426b0ae00170d4f36c3798ae83e1e`；
- atomic acceptance：ADR-0004 和 `RECON-DEC-001`～`003` 已接受，任务验收记录为 `docs/quality/task-acceptance-R0-SPEC-001.json`，backlog 已置 `done`，YYZ manifest 已移除该 open task。Fixture maturity、rights/external distribution、科学资格、R0 gate 与 R1 边界继续 fail-closed。
