# ADR-0004: R0 JSON Schema contracts and dependency-free validation

- Status: Proposed
- Date: 2026-08-10
- Owners: Architecture Lead, Validation Lead
- Related tasks: R0-SPEC-001
- Architecture references: 00A §3.4、05 §10.6、11 §5

## Context

R0 需要在科学与 legacy bundle 扩展前冻结 fixture、oracle 和 `PlanProofRecord` 的机器契约。Bootstrap schema 只约束少量顶层字段，仓库检查也只证明 JSON 可以解析，无法在 identity、provenance、tolerance 或 expected facts 缺失时提前失败。

仓库当前以 C++17、CMake 和 PowerShell 为已接受工具链。许可证策略与通用 JSON/YAML 依赖仍未决定，因此本阶段不引入生产依赖或第三方 schema validator。

first-wave reconciliation 同时提出了三类不兼容候选：同一 v1 名称下更换 `$id` 和字段图、把本地路径与外部 URI 合并为一套 reference grammar、把 `premises` object map 升为 typed proof prerequisite graph。当前仓库已经有四份 fixture manifest、一份 oracle manifest 与多组 proof examples 使用现有 v1，直接混合会让同一版本出现两种解释。

## Decision

R0 契约采用 JSON Schema draft 2020-12 文档，并在每个 instance 中要求稳定的 `schema_version`。fixture 与 oracle schema 显式要求 provenance、expected facts 和 tolerance policy；proof schema 使用蓝图定义的七类 `proof_kind`，并统一结果为 `Proven | Rejected | DeferredUnsupported`。

仓库提供 PowerShell 5.1/7 兼容的验证工具，执行本阶段 schema 使用到的确定子集。验证器遇到未知 schema 关键字时失败。原始 schema/instance JSON 先经过严格读取，拒绝解码后重复的 object key 与非标准 `NaN`、`Infinity`、`-Infinity` token，再交给同一 schema path 验证。

正例、带预期 diagnostic 的反例和实际 manifests 使用同一命令验证；跨实际 manifests 的稳定 ID 唯一性由契约级语义检查补充。fixture authority 必须解析到 role registry，`open_tasks` 只允许解析到未完成 backlog task。`executable`/`qualified` fixture/oracle 的已有 artifact/evidence 字段必须解析到仓库文件；这只是 R0 evidence completeness check，不冻结未来公共 reference URI grammar。

这些 schema 的成熟度为 `Fixture`。`specs/r0-schema-contract-lock.json` 以机器可读形式锁定三份 v1 的 `$id`、instance version、原始字节 SHA-256、字节数和 object field graph。该 lock 只服务 repository validation、fixture/oracle authoring、测试证据和治理评审，产品与 runtime consumer 数量固定为零。

### Proposed reconciliation dispositions

以下 disposition 随本 ADR 保持 `Proposed`，需要 Architecture Lead 与 Validation Lead 对精确 commit/fileset 完成独立复核后才能成为 Accepted decision：

- `RECON-DEC-001 — keep-current`：保留 `https://internal.gnczmkn/schemas/{fixture-manifest|oracle-manifest|plan-proof-record}/1`、`gnczmkn.fixture-manifest/1`、`gnczmkn.oracle-manifest/1`、`gnczmkn.plan-proof-record/1` instance version 和现有字段图。candidate `urn:` identity、扁平 provenance、`oracle_refs` 与 typed oracle fact 不能写入同名 v1。
- `RECON-DEC-002 — repository-root-only`：`executable`/`qualified` fixture 的 `required_artifacts`、fact `evidence_refs` 与 executable/qualified oracle 的 `artifact_refs` 使用仓库根相对、正斜杠、精确大小写的路径。每个 locator 必须解析为 stage-0 tracked、非空、mode 为 `100644` 或 `100755` 的 regular Git blob，并对应非空 regular worktree file。绝对路径、盘符路径、反斜杠、`.`/`..` segment、file URI、未跟踪文件、空 blob、symlink/gitlink 和大小写别名均失败。`source_refs` 继续承载 opaque provenance locator，不由本地 evidence resolver 解释。
- `RECON-DEC-003 — keep-current`：`PlanProofRecord` v1 的 `premises` 保持扁平 object-map scalar snapshot，只记录当前断言的规范化输入事实。typed prerequisite edge、self/unresolved/cycle closure、`PlanProofIndex` 和公共 query contract 不进入 v1；它们需要新版本和 `R2-PRF-001` consumer evidence。

任何 v1 `$id`、instance version、field graph、公共 reference grammar、typed proof prerequisite graph 或 runtime consumer 变化都发布 v2，并同时提供 v1→v2 migration、双版本正反例、独立 validator conformance、consumer evidence 与 superseding ADR。R2 可以在真实 Compiler consumer 和 compatibility evidence 完成后晋升新版本。

## Consequences

- Positive: R0 bundle 在进入科学实现前即可获得确定、跨平台、无网络的失败门禁。
- Positive: schema、examples、实际 manifests 和 CTest 使用同一验证路径。
- Positive: executable evidence 只绑定可由 Git identity 复核的仓库 regular blob，manifest 目录差异不会改变 locator 解释。
- Costs: 仓库内验证器只覆盖已声明子集，新增关键字需要同步实现和反例。
- Costs: 作者需要把 executable evidence 写成仓库根路径；未来外部 evidence reference 需要 v2 contract。
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
- 20 个 contract mutations 覆盖 v1 `$id`/field graph/raw bytes 漂移、manifest-relative/rooted/backslash/drive/file-URI/escape/query/fragment/percent-encoding/untracked/case-alias locator、空/非 regular blob、typed premise overlay、产品 consumer 与 runtime-consumer 计数；
- 至少一个 valid fixture example 使用 HTTP 与 mission URI 形式的 `source_refs`，证明 provenance locator 不进入 executable evidence resolution；
- CTest 与 `tools/verify-repository.ps1` 调用 schema conformance suite；
- Git evidence 查询显式锚定 repository root，从仓库根、构建目录或其他调用目录执行时采用相同 locator 语义；
- `tools/bootstrap.ps1` 在 Windows Debug/Release 通过；新切片的 PowerShell 7/hosted CI 证据在精确 commit 推送前保持 pending。

## Supersession rule

许可证/工具链决定允许采用经过版本锁定的通用 validator，或 R2 Compiler 需要当前子集无法表达的公共契约时，可以提交新 ADR 和 v2 schema identity，并提供双 validator conformance、迁移 fixture 和真实 consumer evidence。三项 proposed reconciliation disposition 只有在独立 commit-bound review 后才随本 ADR 进入 Accepted。
