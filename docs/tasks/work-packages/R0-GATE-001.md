# R0-GATE-001：G0/G1 正式评审准备包

- 状态：Prepared — dependency blocked
- Backlog 状态：`planned`（本准备切片不激活任务）
- Assignee：未指派
- Owner role：Product Owner
- 必须参与：Architecture Lead、Scientific Authority、Validation Lead、Runtime Numerics Lead、Evidence/Workflow Lead
- 准备人：Codex
- 准备日期：2026-08-10
- 直接依赖：`R0-GOV-001`、`R0-GOV-002`、`R0-ARCH-002`、`R0-LEG-002`、`R0-SCI-002`、`R0-SCI-003`、`R0-SCI-004`、`R0-SPEC-001`、`R0-PERF-001`
- 关联 gate：G0（架构压力闭合）、G1（科学 oracle 可用）

## 准备结论

`R0-GATE-001` 当前不能合法激活，也不能形成 `Passed`、`Conditional` 或 `Failed` 的正式 gate decision。它的 9 个直接依赖中，3 个仍为 `review`、6 个仍为 `planned`、0 个为 `done`；任务 assignee 为空，Product Owner、Architecture Lead、Scientific Authority 等 required roles 也都没有具名 assignee/reviewer。仓库只存在 Bootstrap 的 `B0-2026-08-09-passed.md`，不存在 G0 或 G1 的签署记录，`project-manifest.json` 的 `current_gate` 仍是 `R0`。

已有技术证据并非无效：R0 schema、术语/依赖 baseline、Legacy 可复现基线、科学约定 cross-tool suite、license/provenance validator 和候选工具链检查均已实现并通过本地验证。但是，它们分别停留在“技术验证通过但 owner review/ADR 未闭合”或“仅完成实施设计”的层级，不能被提升为 gate pass。特别是：

- 6 份 R0 ADR 仍为 `Proposed`；
- 8 个 required roles 的 16 个 assignee/reviewer slots 均为空；
- hosted CI 仍是 `pending-push-and-run`；
- `R0-ARCH-002` 没有实际 architecture fitness guard/CI integration；
- 7 个 Legacy behavior oracle 全部为 `planned` 且 `artifact_refs` 为空；
- minimal 3DoF、YYZ、CAVH 三个科学 fixture 均为 `specification_only`；
- 没有 benchmark manifest、D0–D3 achieved matrix、approved hardware profile 或 baseline report；
- 未解释科学差异为零尚无可执行 bundle 与差异账本可以证明。

本准备包只定义可复核的 readiness 判定、证据分类、G0/G1 分离决策、签署与 waiver 规则、R1 解锁原子边界和失败 mutation。详细逐项审计见 [R0 G0/G1 readiness audit](../../quality/r0-g0-g1-readiness-audit.md)。它不写入正式 gate decision，不修改 backlog/ADR/role/fixture/oracle/project state，不把准备文档冒充实现，也不代表任何责任人作出批准。

## 权威输入

- [`R0-GATE-001` backlog entry](../backlog.json)：任务 owner、依赖、deliverable、acceptance、evidence 和架构引用的唯一任务来源；
- [11 路线总览 §5](../../../design-notes/gnczmkn-architecture-roadmap/11-roadmap-overview.md)：R0 交付、G0 和 G1 的架构/科学判据；
- [release gates](../../handoff/release-gates.md)、[gate decision template](../../quality/gate-decision-template.md)和 [gate decision 记录规则](../../quality/gate-decisions/README.md)；
- [任务状态规则](../README.md)、`AGENTS.md` 和 `CONTRIBUTING.md`：依赖、assignee、owner、ADR、evidence 和 R0 边界；
- [role assignments](../../team/role-assignments.json)、[team/toolchain readiness report](../../quality/team-toolchain-readiness-report.json)和 ADR-0009；
- ADR-0001–ADR-0009、R0 work packages、schema/architecture/science/provenance reports；
- architecture baseline、PlanProof schema/examples、scientific fixture manifests、Legacy oracle manifest 与 Legacy reproduction evidence；
- `project-manifest.json` 与 `.github/workflows/ci.yml`。

## 当前直接依赖判定

| Task | Backlog | 已有可用证据 | 未闭合项 | Gate 资格 |
| --- | --- | --- | --- | --- |
| `R0-GOV-001` | `review` | 角色/工具链 validator、候选 CI profile、11 个 mutation | 16 个 required role slots；ADR-0009；hosted run | 不合格 |
| `R0-GOV-002` | `review` | provenance inventory、policy validator、8 个 mutation | ADR-0008；Product Owner 决策；distribution license 未选 | 不合格 |
| `R0-ARCH-002` | `planned` | fitness coverage 设计和 20 个 mutation 设计 | production guards、positive/negative suite、CI integration | 不合格 |
| `R0-LEG-002` | `planned` | 7 类 oracle capture 设计 | executable artifacts、tolerance、Preserve/Fix/Retire 审批 | 不合格 |
| `R0-SCI-002` | `planned` | minimal 3DoF reference 设计 | executable independent trajectory/convergence/failure bundle | 不合格 |
| `R0-SCI-003` | `planned` | YYZ bundle 设计 | authoritative source/assets、independent values、machine-valid evidence chain | 不合格 |
| `R0-SCI-004` | `planned` | CAVH formula 设计 | authoritative source edition/license、independent formula bundle | 不合格 |
| `R0-SPEC-001` | `review` | 4 个 schema、valid/invalid examples、validator | Architecture Lead review；ADR-0004 acceptance | 不合格 |
| `R0-PERF-001` | `planned` | determinism/performance baseline 设计 | workload、profiles、raw samples、budgets、baseline report | 不合格 |

`R0-ARCH-001`、`R0-LEG-001` 与 `R0-SCI-001` 虽不是 gate 的直接依赖，但分别是上述任务的上游且仍为 `review`。这些技术切片不能通过把下游任务标记 `done` 来绕过 owner 审查。

## 本次审计锚点

- 被审计实现 commit：`bfd735e38a0d3f84fbe19f0f93435522cbdf7d56`
- backlog raw SHA-256：`8627b65f1d655f09043833eb23965c3bf72772e554eedc9e920429d131f3a06c`
- 11 路线总览 raw SHA-256：`5ec5e82cbb3339027b58d3ad9b066c5a569b742cbffaf0dcb241cb85b8664dc9`
- release gates raw SHA-256：`99fcaa4e387e90b233554d54d50ff8ace4575298524d20eac198d737aed7a22b`
- gate template raw SHA-256：`143c656914b6ff577e0ad06a4f5613e5c81c596daf7dba031134f9fbf94cc2d9`
- role assignments raw SHA-256：`4ede05482e214268cc7cf0dc139c17f0e92b9bf9e8c773b090ce83d6fe678f0e`
- team/toolchain report raw SHA-256：`66baec53e7077a880aaa7b971ff13fbbd6bcaf69576aecd6110b5ec04aedb5c1`
- architecture baseline raw SHA-256：`237d721a1cbd6737b293472475e2233f89e56fa58a3f62c7feb3c89506d76f7b`
- terminology report raw SHA-256：`fdba6dba1c0f802da2fd86df86c8b6c549edfaa1d093626e3685974f0816bf82`
- oracle manifest raw SHA-256：`eade78296e7b102db4e2ca59aec503df8b2a63db1d2810851909a375bd64d0b8`
- scientific cross-tool report raw SHA-256：`6db8218e5d52cecffc474f2851c10c4089976292a3b7396350233063a66a3419`
- license/provenance report raw SHA-256：`f6f11cd0fb064203465377f18beb91b865cc31d590ebf88816427e1f221c7d3b`
- hosted CI workflow raw SHA-256：`33a6afa5b2adfbcb4143b142fd56cff2ecb10e2625c2f9f79947fbd4e94198e2`

这些 hash 固定准备审计读取的 raw bytes；它们不是 gate pass，也不能替代未来正式评审对 reviewed commit 的重新捕获。

## Gate 证据状态词

正式评审前必须区分以下状态，禁止只用“存在/不存在”或总分掩盖差异：

| 状态 | 定义 | 能否满足 gate criterion |
| --- | --- | --- |
| `verified_technical` | 可执行验证在声明 profile 上通过，输入和输出可追溯 | 仅在 owner/ADR/dependency 条件也闭合时可用 |
| `review_pending` | 实现和技术证据存在，但有权 reviewer/ADR 尚未批准 | 否 |
| `prepared_only` | 问题、接口、测试和 mutation 已设计，production artifact 尚未实现 | 否 |
| `missing` | 必须 evidence 不存在或不可重放 | 否 |
| `rejected` | 有权 reviewer 明确拒绝且记录影响/替代方案 | 只可用于 acceptance 明确允许“closed or explicitly rejected”的项；不能伪装成实现 |
| `waived` | 有具名风险接受人、scope、期限和关闭任务的限时豁免 | 依 gate/criterion；科学未解释差异不能据此通过 G1 |
| `invalid_for_gate` | 来源不独立、hash 不闭合、profile 不匹配、签署无权或被聚合掩盖 | 否 |

## G0/G1 必须分开作出决定

1. G0 先审架构闭包、变化分流、PlanProof、依赖守卫和广域压力反证；
2. 只有 G0 `Passed` 后，才进入可解锁意义上的 G1 科学验收；
3. G1 审 quaternion property、minimal 3DoF、YYZ、CAVH、Legacy 分类、schema/conformance 与科学差异；
4. 两个 gate 各有独立 decision record、reviewed commit、criteria table、waiver 与批准人；
5. G0 `Conditional` 不得解锁会消费缺失架构契约的 G1 工作；G1 `Conditional` 不得解锁 R1；
6. “尚未具备正式评审条件”记录为 readiness 的 `not_ready_for_authorized_review`，不能擅自生成 `G0-...-failed.md` 或 `G1-...-failed.md`；
7. 正式 `Failed` 只能由已召集且具名授权的 gate review 对冻结 commit 作出。

## 正式评审程序

1. 选择干净、可获取、所有候选证据均为其祖先或由其可重放生成的 reviewed commit；
2. 校验 `R0-GATE-001` 的 9 个直接依赖全部为 `done`，并核对各自 deliverable、acceptance、evidence 与 owner approval；
3. 校验 required role 的 assignee/reviewer 完整、独立且非 placeholder；固定 G0/G1 评审人；
4. 校验 R0 所依赖 ADR 的状态为 `Accepted`，记录确切 bytes/hash；
5. 生成 evidence index，固定 authority input、fixture/oracle/raw artifact/report/test/binary/profile 的 SHA-256 与 lineage；
6. 在批准的 Windows/MSVC、Ubuntu/GCC 和所需 dedicated profile 上执行 repository、Debug/Release、schema、architecture、Legacy、science、determinism/performance 验证；保存 hosted run identity；
7. 对照 11 §5.1/§5.2 完成 G0 criterion 和压力样本表；每项必须有可打开 evidence，不能仅引用设计蓝图；
8. Architecture Lead 给出 G0 结论；只有 `Passed` 才继续能解锁意义上的 G1；
9. 对照 11 §5.3 完成 G1 scientific bundles、独立性证明、Legacy Preserve/Fix/Delete 分类与差异账本；`unexplained` 必须为 0；
10. 执行 approved determinism/performance/capacity profile，不能用 Legacy wall time 或 skeleton 启动时间代替；
11. 对每个 waiver 记录 scope、criterion、风险、consumer、owner、expiry、关闭任务和“能否解锁”；
12. Architecture Lead、Scientific Authority、Validation Lead、相关 domain owners 与 Product Owner 按职责签署；
13. 分别新增 G0、G1 decision records；只有 G1 `Passed` 才在同一授权变更中更新 backlog、`project-manifest.current_gate` 和精确 R1 unlock set；
14. 更新后重新验证 clean checkout，并保存 decision commit 与所有 artifact hash。

## 正式 decision 的最小证据索引

每份 decision record 至少可追到：

- gate id、结果、review date、reviewed commit、decision commit；
- 当前 gate、前置 gate、解锁范围和明确未解锁范围；
- 每个 criterion 的 stable id、authority ref、result、evidence refs/hash、owner/reviewer；
- 直接/间接任务状态、work package commit、acceptance 和 evidence closure；
- ADR id/status/hash 与 role assignment snapshot/hash；
- fixture/oracle id、status、source/input hash、reference independence、expected/actual、tolerance/comparator；
- build/toolchain/OS/compiler/FP/runtime profile、binary hash、command、test/run id；
- raw artifact、report、artifact index 与 lineage hash；
- scientific difference id/classification/approver/disposition；
- waiver id/scope/risk/owner/expiry/closure task/unlock effect；
- 每个签署人的 role、identity、decision 和 timestamp。

## 必测失败路径

| Mutation | 注入 | 预期拒绝 |
| --- | --- | --- |
| `GATE-MUT-001` | reviewed commit 缺失、不可获取或评审后 tree 有未记录漂移 | commit-integrity gate |
| `GATE-MUT-002` | `review`/`planned` 依赖被当作 `done` | dependency-state gate |
| `GATE-MUT-003` | preparation design 被当作 executable artifact | evidence-status gate |
| `GATE-MUT-004` | validator pass 被当作 owner approval | authority gate |
| `GATE-MUT-005` | `Proposed` ADR 被当作 `Accepted` | decision-status gate |
| `GATE-MUT-006` | required role 为空、同人自审或 placeholder 签署 | role/independence gate |
| `GATE-MUT-007` | hosted CI `pending` 被当作 passed run | execution-evidence gate |
| `GATE-MUT-008` | `specification_only` fixture 被当作 executable | fixture-status gate |
| `GATE-MUT-009` | planned oracle 的 `artifact_refs` 为空仍通过 | oracle-completeness gate |
| `GATE-MUT-010` | Legacy output/hash 被当作独立科学真值 | reference-independence gate |
| `GATE-MUT-011` | scientific difference `unexplained > 0` 仍通过 G1 | scientific-difference gate |
| `GATE-MUT-012` | report 只有结论而无 raw artifact/hash/command/profile | evidence-lineage gate |
| `GATE-MUT-013` | G1 在 G0 未 Passed 时解锁 | gate-order gate |
| `GATE-MUT-014` | `Conditional` 决定解锁 R1 | unlock-policy gate |
| `GATE-MUT-015` | waiver 缺 owner、expiry、scope、risk 或 closure task | waiver-completeness gate |
| `GATE-MUT-016` | aggregate pass 掩盖单项 criterion fail/missing | criterion-completeness gate |
| `GATE-MUT-017` | 缺 workload/profile 的性能数字被当作 baseline | performance-evidence gate |
| `GATE-MUT-018` | `NOASSERTION`/内部默认被当作对外分发授权 | provenance/license gate |
| `GATE-MUT-019` | Codex、自审文字或无权人员被当作 owner signature | signature-authority gate |
| `GATE-MUT-020` | decision、backlog、manifest、unlock 非原子或未授权更新 | release-state gate |

## 本准备切片保持零修改

- `docs/tasks/backlog.json` 的状态、assignee、依赖、acceptance 和 unlock；
- ADR 状态、role assignments、toolchain qualification 和 hosted CI 状态；
- 正式 `docs/quality/gate-decisions/G0-*`、`G1-*` 记录；
- `project-manifest.json` 的 `current_gate`；
- schema、fixture/oracle status、scientific expected、tolerance 和 difference classification；
- `.github/workflows/ci.yml`、CMake、产品模块、Legacy archive/source/evidence；
- R1–R8 production capability、manager/plugin/callback/compatibility surface；
- waiver、批准人、签名或 distribution license 决策。

## 激活前置条件

1. 9 个直接依赖由各自有权 reviewer 关闭为 `done`，其 deliverable、acceptance 和 evidence 可重放；
2. 上游 `R0-ARCH-001`、`R0-LEG-001`、`R0-SCI-001` 的 review/ADR 也合法闭合；
3. Product Owner 为 `R0-GATE-001` 指派 assignee 和 review date；
4. required roles 与独立 reviewers 完整，ADR-0004–ADR-0009 已接受或被有权者明确拒绝并处理影响；
5. hosted CI 和必要 dedicated profile 有 retained run evidence；
6. G0/G1 evidence index、criteria matrix、scientific difference ledger 与 waiver ledger 已生成且 hash 闭合；
7. reviewed commit 冻结、tree clean、所有命令可从 clean checkout 重放；
8. 不存在用准备文档、Legacy 或 aggregate score 替代 executable/independent evidence 的条目。

## 完整验收证据

- G0 与 G1 各有符合命名规则的具名签署 decision record；
- 9 个直接依赖均为 `done`，每个 acceptance/evidence 逐项闭合且 owner approval 可定位；
- 11 §5.1 的 R0 deliverables 与 §5.2/§5.3 的每条 criterion 都有 stable id、结果和 artifact hash；
- G0 压力反证、fitness positive/negative suite 和无 product-name Kernel branch 证据通过；
- quaternion、minimal 3DoF、YYZ、CAVH、Legacy oracle、REF-YYZ schema chain 全部可执行且 reference 独立；
- scientific difference ledger 的 `unexplained` 为 0；
- determinism/performance/capacity 结果绑定 approved workload/profile/raw samples；
- role、ADR、provenance/license、hosted CI、waiver 与 artifact lineage 完整；
- G1 结果必须为 `Passed` 才可产生精确 R1 unlock；
- decision/backlog/project manifest/unlock 在授权 commit 中一致，clean checkout 验证通过。

## 准备切片审查记录

- 实现自审：Codex，2026-08-10；结论为“已把直接依赖、G0/G1 criterion、证据状态、签署/waiver/unlock 规则和 20 项 mutation 收敛为可审查准备包，未生成虚假 gate decision”。本自审不替代任何 owner review。
- 状态审查：9 个直接依赖为 3 `review` + 6 `planned` + 0 `done`；`R0-GATE-001` 为 `planned`、assignee 为空。
- 架构审查：已有 baseline/PlanProof schema 是部分技术证据；architecture fitness implementation、压力样本闭包和具名 Architecture Lead review 缺失，G0 未就绪。
- 科学审查：只有 convention fixture 为 `executable`；minimal 3DoF、YYZ、CAVH 为 `specification_only`，Legacy oracles 为 `planned`，G1 未就绪。
- 治理审查：6 个 Proposed ADR、16 个缺失 role slots、hosted CI pending；Codex 不代签、不改状态。
- 发布审查：没有新增 G0/G1 decision、waiver 或 R1 unlock；`project-manifest.current_gate` 保持 `R0`。
- 边界审查：本切片只增加 work package 与 readiness audit，不修改产品、schema、CI、Legacy 或权威科学数据。
