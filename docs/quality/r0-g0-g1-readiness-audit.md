# R0 G0/G1 readiness audit

- Audit id：`R0-GATE-READINESS-20260810-001`
- Task：`R0-GATE-001`
- Audited implementation commit：`bfd735e38a0d3f84fbe19f0f93435522cbdf7d56`
- Prepared by：Codex
- Prepared on：2026-08-10
- Current-state amendment：2026-08-12，through R0-SPEC-001 and R0-ARCH-001 commit-bound acceptance
- Readiness verdict：`not_ready_for_authorized_review`
- Official gate result：无；本文件不是 G0/G1 decision record
- Current project gate：`R0`

## 1. 执行摘要

当前仓库已经从纯蓝图进入“R0 基础契约可执行、后续工作可精确领取”的状态，但尚未达到 G0 或 G1 的正式评审入口条件。

最硬的机器事实是：[`R0-GATE-001`](../tasks/backlog.json) 的 9 个直接依赖中，1 个为 `review`、6 个为 `planned`、2 个为 `done`。依赖未全部 `done`、gate assignee 未填写，因此仍不能领取 gate 任务。8 个 required roles 的 assignee/reviewer、ADR-0009 和受支持 Hosted CI 已闭合；ADR-0004/R0-SPEC-001 与 ADR-0005/R0-ARCH-001 均已通过 commit-bound 接受。其余架构、科学、Legacy、性能与 rights/provenance 输入继续阻止正式评审。

G0 有可复用的部分基础：ADR-0003 已接受，9-module CMake DAG、术语/owner/Legacy migration baseline、PlanProofRecord fixture schema 和 negative examples 可执行；但 architecture fitness functions 仍只有设计，ChangeCard/CapabilitySlice/广域压力样本没有 gate fixture，PlanProofIndex/YYZ dry-run/十三压力面/表示矩阵没有独立验收 artifact，因此不能证明架构压力闭合。

G1 只有科学约定 bundle 达到 `executable`，且其技术 cross-tool comparison 为 0 mismatch。minimal 3DoF、YYZ、CAVH manifests 仍为 `specification_only`；7 个 Legacy oracles 仍为 `planned` 且 artifact refs 全空；没有 machine-valid REF-YYZ 全链、独立轨迹/公式真值、已批准 tolerance/disposition 或 scientific difference ledger。因此不能证明“未解释科学差异为零”。

本审计的作用是防止三类最危险的假阳性：

1. 把蓝图、准备设计或 manifest 声明当作已实现 capability；
2. 把 validator/CTest 通过当作 Product Owner、Architecture Lead 或 Scientific Authority 批准；
3. 把 Legacy 自洽、aggregate pass 或 `Conditional` 当作独立科学真值和 R1 unlock。

## 2. 判定边界

### 2.1 readiness 不是 gate result

本审计使用 `not_ready_for_authorized_review`、`ready_for_authorized_review` 两种 readiness 结论。它们只回答“是否具备召集正式评审的完整输入”，不回答 gate 最终是否 `Passed`、`Conditional` 或 `Failed`。

正式 gate result 必须：

- 针对冻结且可获取的 reviewed commit；
- 由具名且被授权的 reviewer 作出；
- 引用完整 criterion/evidence/hash/waiver/difference；
- 写入 `docs/quality/gate-decisions/G<id>-YYYY-MM-DD-<result>.md`；
- 由 decision commit 固化。

未满足这些条件时创建 `G0-...-failed.md` 也不正确，因为那会伪造一次从未合法召集的正式 decision。

### 2.2 证据状态

| 状态 | 精确定义 | 对 criterion 的默认效果 |
| --- | --- | --- |
| `verified_technical` | production validator/test 在声明 profile 上执行成功，输入/输出/hash 可追溯 | 候选通过；仍需 dependency/ADR/owner closure |
| `review_pending` | 实现与技术证据完整，但有权 owner/reviewer 或 ADR 尚未关闭 | `not_met` |
| `prepared_only` | 已有问题定义、设计、实施切片和 mutation，没有 production artifact | `not_met` |
| `missing` | required evidence 不存在、不完整或无法重放 | `not_met` |
| `rejected` | 有权 owner 对要求/方案作出显式拒绝，影响与替代路径已记录 | 只按 backlog acceptance 的明确规则处理 |
| `waived` | 限定 scope/期限/风险接受人/关闭任务的正式 waiver | 由 gate policy 决定；不能清除 G1 unexplained difference |
| `invalid_for_gate` | 来源不独立、profile 不可比、hash/lineage 缺失或签署无权 | `not_met` |

存在一个文件不等于存在证据。例如 `fixture-manifest.json` 的 `status: specification_only` 只证明 bundle 契约已经规划，不证明 reference 已生成。

### 2.3 criterion 计算规则

一个 criterion 只有同时满足下列条件才能标记 `met`：

1. authority requirement 可定位且没有未裁决冲突；
2. required task/dependency 为 `done`；
3. implementation/reference artifact 实际存在；
4. validator/test 对冻结 bytes、批准 profile 执行；
5. expected/actual/comparator/tolerance/result 可定位；
6. provenance、hash、lineage 与 replay command 闭合；
7. domain owner/reviewer 已批准；
8. waiver/difference 不违反该 gate 的 hard rule。

任何一项缺失都必须保留为单项 `not_met`，不能被其他项目的 aggregate pass 抵消。

## 3. 审计锚点

下列 SHA-256 是对审计时 raw bytes 的固定，不是未来 gate decision 的替代物。

审计后发现远端还存在从 B0 独立分叉的 `origin/codex/r0-first-wave@7d7c0a8`。其技术资产、hosted run、虚拟角色签署、schema/ADR 冲突和四元数 axis-angle 差异已单独记录在 [remote first-wave reconciliation audit](r0-first-wave-reconciliation-audit.md)。该分支证据不改变本文件对当前线的 task/ADR/role/fixture readiness 判定。

| Input | SHA-256 |
| --- | --- |
| `docs/tasks/backlog.json` | `8627b65f1d655f09043833eb23965c3bf72772e554eedc9e920429d131f3a06c` |
| `design-notes/.../11-roadmap-overview.md` | `5ec5e82cbb3339027b58d3ad9b066c5a569b742cbffaf0dcb241cb85b8664dc9` |
| `docs/handoff/release-gates.md` | `99fcaa4e387e90b233554d54d50ff8ace4575298524d20eac198d737aed7a22b` |
| `docs/quality/gate-decision-template.md` | `143c656914b6ff577e0ad06a4f5613e5c81c596daf7dba031134f9fbf94cc2d9` |
| `docs/quality/gate-decisions/README.md` | `086e1147f6c16b7844df533ac6846065072338b60e35f22be85ffdef719287ab` |
| `docs/tasks/README.md` | `082367084a54b50dcaa886596e0badc643e59222cf6ece425e4cf79d9cbce110` |
| `project-manifest.json` | `eb8a1145d95c92396073a4a9ece7d5d41f84b2e286c0842331c1d9e8653ca5ad` |
| `docs/team/role-assignments.json` | `4ede05482e214268cc7cf0dc139c17f0e92b9bf9e8c773b090ce83d6fe678f0e` |
| `docs/quality/team-toolchain-readiness-report.json` | `40d0ce185e49f2a63a74f05802a48462f9e75490fe2f2e825d37d4c3bcec4849` |
| `docs/architecture/architecture-baseline.json` | `69cf9c4f26a15b62ecdb60364417bb066c89756924134612a3701471f10943e7` |
| `docs/quality/terminology-conformance-report.json` | `298f06e15eb7b52d37a9a6c0338222d2a6e88ec2991301a1680acb2b4e8527fc` |
| `oracles/oracle-manifest.json` | `eade78296e7b102db4e2ca59aec503df8b2a63db1d2810851909a375bd64d0b8` |
| `REF-SCIENTIFIC-CONVENTIONS-001` manifest | `446d6700f3e7fc86ae4a6905586b471dea7bd1bec4b0c7a72e8c3777491026ef` |
| `REF-MINIMAL-3DOF-001` manifest | `8e90bdefcf8d5250f22b147a061e7a5934c9618d08bc9f63e6407a32732f579a` |
| `REF-YYZ-001` manifest | `69eb288d48714edcdc565268151e151cbb62a4e1a2f8644a45c000d659f5086f` |
| `REF-CAVH-FORMULA-001` manifest | `d89f4e9517aeb0e6a7eebecf3191b37aef29a1a875a5e125f9db72007e891ac6` |
| scientific conventions report | `c121c74b546b2ad7722a6a5d90ee8ca0de028c4524fce624a3b95963138252c8` |
| license/provenance report | `d47def50dd372ddf68a334917da4b93f070c582beac07b50db82e9abb97a1284` |
| `.github/workflows/ci.yml` | `33a6afa5b2adfbcb4143b142fd56cff2ecb10e2625c2f9f79947fbd4e94198e2` |

正式评审必须针对新的 reviewed commit 重新计算全部 authority/evidence hash；不能把本表复制为永久真值。

## 4. R0 任务与实施状态

### 4.1 Gate 直接依赖

| Task | Owner role | Backlog / assignee | 当前切片 | 可验证事实 | 关闭前缺口 |
| --- | --- | --- | --- | --- | --- |
| `R0-GOV-001` | Product Owner | `done` / `r0-po-agent` | commit-bound acceptance | 8/8 role pairs、ADR-0009、受支持 Hosted CI 与 49 governance mutations 全部闭合 | 已闭合 |
| `R0-GOV-002` | Product Owner | `review` / Codex | `2796dbc` + `7d43a64` | inventory bytes/8 `NOASSERTION` facts 不变；closed categories、generated lineage/restriction；14 mutations matched/rejected | ADR-0008、license/share authority decisions；scientific context remains `RECON-DEC-005` |
| `R0-ARCH-002` | Architecture Lead | `planned` / null | prep `f3b2bc1` | guard coverage、实施分层和 mutations 已设计 | production guard、test、CI evidence |
| `R0-LEG-002` | Validation Lead | `planned` / null | prep `4c62bc2` | 7 类 capture/probe/comparator/disposition 已设计 | executable oracle artifact refs 全部缺失 |
| `R0-SCI-002` | Model SDK Lead | `planned` / null | prep `4a610a0` | analytic/independent 3DoF bundle 设计 | source、expected trajectory、convergence/failure runs |
| `R0-SCI-003` | Scientific Authority | `planned` / null | prep `1c77089` | REF-YYZ source-to-evidence 设计 | authoritative inputs、independent expected、完整 artifacts |
| `R0-SCI-004` | Scientific Authority | `planned` / null | prep `41623ee` | formula lanes/cases/derivative/envelope/mutations 设计 | source edition/license、independent values、executable suite |
| `R0-SPEC-001` | Architecture Lead | `done` / `r0-architecture-agent`；reviewer `r0-validation-agent` | technical target `ee715735` | 3 contracts；6 valid、16 invalid、9 strict failures、25 identities、5 identity mutations、20 contract 与 25 acceptance mutations；双平台 CI | 已闭合 |
| `R0-PERF-001` | Runtime Numerics Lead | `planned` / null | prep `bfd735e` | correctness/determinism/capacity/perf 分离设计 | workload/profile/raw samples/budgets/baseline |

直接依赖统计：`review=1`、`planned=6`、`done=2`。因此 gate task activation 的必要条件仍为 false。

### 4.2 重要间接依赖

| Task | Backlog | Commit | 有效技术证据 | 仍缺 |
| --- | --- | --- | --- | --- |
| `R0-ARCH-001` | `done` / `r0-po-agent`；reviewer `r0-validation-agent`；decision owner `r0-architecture-agent` | technical target `29f455efebd72113c1d311bc674a78c638265f34` | 276 terms、20 aliases、10 capabilities、27 shared symbols、22 Legacy mappings、9 modules、22 CMake edges；15 architecture + 35 review-contract + 76 acceptance mutations；33 responsibilities 完整分类；双平台 commit-bound CI | 已闭合：ADR-0005、`RECON-DEC-006/007` 与 task acceptance 均 accepted |
| `R0-LEG-001` | `review` | `b2e69d0` | frozen archive reproduction、双跑 normalized hash | Validation/Science owner classification/review |
| `R0-SCI-001` | `review` | `9e95f1c` + `4c916e1` | C++/Python 各 23 checks/1819 assertions、16 observations、0 mismatch、6 failures rejected | ADR-0006/0007、Scientific Authority review；half-open validity remains Proposed |

它们是 direct dependencies 的上游。下游不能通过状态修改来绕过这些 review。

### 4.3 已实现不等于已接受

R0 当前有 3 个任务完成 commit-bound acceptance，3 个技术任务处于 `review`，另有 6 个 dependency-blocked `planned` 任务；`R0-ARCH-002` 的依赖已闭合，当前保持未指派并等待合法激活。此区分是有意的：

- implementation commit 证明已登记 implementation actor 交付了可审代码/证据；
- `review` 表示 acceptance 和有权审批尚未闭合；
- preparation commit 只降低未来实施歧义，不激活 `planned` task；
- 只有 owner/reviewer 闭合 deliverable、acceptance、evidence 后才允许 `done`。

## 5. 治理与决策 readiness

### 5.1 ADR 状态

| ADR | Status | Gate 影响 |
| --- | --- | --- |
| ADR-0001 Greenfield/Legacy reference | `Accepted` | Legacy 只读/evidence-only 边界可用 |
| ADR-0002 C++17/CMake modular monolith | `Accepted` | 基础构建形态可用 |
| ADR-0003 initial module DAG | `Accepted` | 当前 module dependency authority 可用 |
| ADR-0004 R0 JSON schema contracts | `Accepted` | Fixture v1 identity/field graph、repository-root evidence 与 PlanProof v1 边界已接受 |
| ADR-0005 derived architecture baseline | `Accepted` | authority/derivation policy、两个逻辑边界与 22 条 owner/consumer map 已冻结 |
| ADR-0006 SI/frame/time conventions | `Proposed` | G1 convention policy 未获 Scientific Authority 接受 |
| ADR-0007 passive Hamilton quaternion | `Proposed` | G1 quaternion convention 未获 Scientific Authority 接受 |
| ADR-0008 internal-default license/provenance | `Proposed` | 分享/分发资格不能从 validator pass 推导 |
| ADR-0009 roles/candidate toolchain | `Accepted` | accountable review 与 supported toolchain 已冻结；不覆盖 performance profile 或 rights |

正式 gate 必须固定依赖 ADR 的准确集合，且逐份记录 status/hash/approver。`Proposed` 不得按“代码已经使用它”隐式升级。

### 5.2 角色与评审独立性

`docs/team/role-assignments.json` 当前：

- 9 roles，共 8 required；
- required assignee 已填 8；
- required reviewer 已填 8；
- valid required pairs 8；
- missing required slots 0；
- 四个 actor 使用四个唯一 task binding，Scientific Authority 与 Architecture Lead 在 actor/task 两层独立。

Gate 最低职责分离：

| 决策面 | Accountable decision | 必须 review |
| --- | --- | --- |
| G0 overall / R1 unlock | Product Owner | Architecture、Science、Validation 和受影响 domain leads |
| G0 architecture criteria | Architecture Lead | Validation Lead；高风险科学接缝含 Scientific Authority |
| G1 conventions/reference/tolerance/difference | Scientific Authority | Validation Lead；模型实现含 Model SDK Lead |
| Legacy behavior facts/disposition | Validation Lead + corresponding domain owner | Scientific Authority / Architecture Lead 按事实类型 |
| determinism/performance/capacity | Runtime Numerics Lead | Validation Lead、Scientific Authority、Compiler Lead |
| artifact/evidence lineage | Evidence/Workflow Lead | Validation Lead |
| license/external sharing | Product Owner | Architecture Lead 与 provenance reviewer |

当前责任主体均以 `machine_agent` 明确披露并解析到仓库所有者授权；未登记别名、身份冒充和同一 actor/task 自审继续无效。

### 5.3 CI 与工具链

当前 correctness/conformance 工具链矩阵已获得 commit-bound Hosted CI：

- Windows/MSVC 与 Ubuntu/GCC 两个固定 profile 的 qualification status 为 `supported`；
- Hosted CI receipt 永久保存 run/job、runner/tool/compiler identity、必需步骤、日志 hash 与保留策略；
- rolling hosted hardware 不足以提供稳定 performance baseline；
- G0/G1 正式 evidence 应分别声明 correctness/conformance matrix 与 dedicated performance profile；
- compiler、OS、flags、FP environment、binary hash、run id 缺任一项时，不得声称跨工具链 determinism。

### 5.4 License/provenance

license/provenance validator 的 `status: passed` 表示“policy 能拒绝不完整或越权输入”，并不表示仓库已获得分发许可。报告同时明确：

- ADR-0008 为 `Proposed`；
- repository distribution license 未选择；
- root distribution license files 为 0；
- inventory 8 项均为 `NOASSERTION`；
- 4 项 external blocked、4 项 external not redistributed；
- Product Owner 与 Architecture Lead 已指派；license/right 决定本身仍未完成。

因此 G1 所需 paper/table/data/source 必须逐项解决 citation、edition、license、redistribution 和 expected-value derivation；不能把内部默认当作外部授权。

## 6. G0 readiness matrix

Authority：[11 §5.1](../../design-notes/gnczmkn-architecture-roadmap/11-roadmap-overview.md) R0 交付与 §5.2 G0 退出门。

### 6.1 R0 架构交付

| Criterion | 要求 | 当前证据 | 状态 | 关闭条件 |
| --- | --- | --- | --- | --- |
| `G0-D-001` | 三道 firewall + 五 partitions dependency map | 蓝图；ADR-0003；Accepted ADR-0005；9-module CMake DAG baseline | `prepared_only` | `R0-ARCH-002` 实现 firewall/partition route 逐项 conformance |
| `G0-D-002` | ChangeCard + `<AuthorityDomain, Delta<V,G,S,T,I,R,X>>` | 蓝图术语；baseline capability/change-vector data | `missing` | machine-valid filled samples、normal/invalid cases、owner review |
| `G0-D-003` | 四类 closed operation languages + Model lowering | 蓝图术语与 operator inventory | `missing` | grammar/closed-set fixture、lowering examples、unsupported diagnostics |
| `G0-D-004` | 7 类 PlanProofRecord | Accepted Fixture v1 schema enum、3 result-shape valid + 5 result-shape invalid examples、schema test | `partial` | 每个 proof kind 的 positive/negative semantic coverage 与后续 PlanProofIndex consumer evidence |
| `G0-D-005` | A–F classification + 9 seams | 蓝图与 baseline vocabulary | `missing` | filled A–F cases、seam ownership/route/untouched assertions |
| `G0-D-006` | source ownership/deletion map | Accepted authority registry、22 Legacy migrations、baseline validator | `prepared_only` | `R0-ARCH-002` 实现 source path/deletion guard 与证据闭合 |
| `G0-D-007` | scientific bundles | manifests + preparation designs | `prepared_only` | 见 G1 matrix；executable bundles |
| `G0-D-008` | 7 Legacy behavior oracle tests | planned manifest + capture design | `prepared_only` | 7 executable artifacts、hash/tolerance/disposition/negative tests |
| `G0-D-009` | terminology conformance checker | 276 terms、20 aliases、27 shared symbols、15 baseline + 35 review-contract + 76 acceptance negative cases；report conformant | `verified_technical` | `closed`：ADR-0005 与 R0-ARCH-001 accepted/done |
| `G0-D-010` | PlanProofRecord/Index + YYZ fixture + dry-run | Record schema 和单个 guidance-rate example | `missing` | Index schema/query、YYZ proof set、dry-run expected/actual evidence |
| `G0-D-011` | 13 pressure route/untouched table | blueprint + fitness coverage design | `prepared_only` | executable/validated table，13/13 route 与 untouched assertions |
| `G0-D-012` | representation matrix + causal walkthroughs | blueprint narrative | `missing` | machine-reviewable cases covering required consumers/physical scenarios |
| `G0-D-013` | explicit deferred Segment/Topology/dynamic ABI | roadmap + baseline capability matrix | `review_pending` | guard against production definitions/callback substitutes；owner review |
| `G0-D-014` | architecture fitness functions | R0-ARCH-002 coverage design | `prepared_only` | production guard suite + CI integration + positive/negative evidence |

### 6.2 G0 退出判据

| Criterion | 反证问题 | 当前状态 | 关键缺口 |
| --- | --- | --- | --- |
| `G0-X-001` | 普通 A–E 是否都能填写 ChangeCard | `missing` | 没有 filled case corpus/validator |
| `G0-X-002` | F/cross-domain/critical changes 是否拆为 CapabilitySlice | `missing` | 没有 machine-valid high-risk slices |
| `G0-X-003` | 每个 high-risk slice 是否映射 authority/vector/grammar/proof/operator/commit/evidence | `missing` | 仅蓝图推导，无 end-to-end evidence |
| `G0-X-004` | cross-domain handoff 是否全可追踪 | `prepared_only` | coverage plan 尚未实现 lineage/route assertions |
| `G0-X-005` | Kernel 是否无 product-name branches | `prepared_only` | 当前 skeleton 无此分支，但缺未来 regression guard；“代码少”不是闭包证明 |
| `G0-X-006` | formats/controls/faults/multi-entity/separation/ground/constellation/high-fidelity/MC/RL/LLM/report/HIL/realtime 是否同规则闭包 | `prepared_only` | 无 approved sample corpus、expected routes、negative counterexamples |
| `G0-X-007` | unknown topology/step jump 是否显式 deferred | `review_pending` | baseline 有 Deferred 记录，仍缺 production guard/unsupported evidence |
| `G0-X-008` | 是否无临时 callback/global-container hacks | `prepared_only` | 当前 skeleton 未实现 hack，但缺 source/semantic guard 与 mutation |

### 6.3 G0 blocker ledger

| Blocker | Owner | Close evidence |
| --- | --- | --- |
| `G0-BLK-001` ARCH-001/ADR-0005 owner review | Architecture Lead | `closed`：ADR-0005 + R0-ARCH-001 accepted/done，技术文件集已固化 |
| `G0-BLK-002` schema authority pending | Architecture Lead | `closed`：ADR-0004 + SPEC-001 accepted/done |
| `G0-BLK-003` architecture fitness implementation absent | Architecture Lead | ARCH-002 guard suite/CI report |
| `G0-BLK-004` ChangeCard/CapabilitySlice corpus absent | Architecture Lead | valid/invalid fixtures + route results |
| `G0-BLK-005` PlanProofIndex/YYZ dry-run absent | Architecture Lead + Compiler Lead | schema/query fixture/report |
| `G0-BLK-006` broad pressure counterexamples absent | Architecture Lead + affected domain leads | 13-surface plus withheld scenario evidence |
| `G0-BLK-007` role/CI governance incomplete | Product Owner + Validation Lead | `closed`：GOV-001 done + retained Hosted CI |

当前 G0 readiness：`not_ready_for_authorized_review`。

## 7. G1 readiness matrix

Authority：[11 §5.3](../../design-notes/gnczmkn-architecture-roadmap/11-roadmap-overview.md) 与 backlog direct dependencies。G1 必须在 G0 `Passed` 后才可产生解锁效力。

| Criterion | 要求 | 当前证据 | 状态 | 关闭条件 |
| --- | --- | --- | --- | --- |
| `G1-X-001` | 修订 quaternion convention 有 properties | executable convention fixture；C++/Python 各 23 checks/1819 assertions、0 mismatch、6 failures rejected | `review_pending` | ADR-0006/0007 + Scientific Authority review；axis-angle/validity owner decisions；approved profiles rerun |
| `G1-X-002` | minimal 3DoF 独立 initial/trajectory/termination | specification-only manifest + detailed design | `prepared_only` | executable source/reference/trajectory/termination/convergence/failure bundle |
| `G1-X-003` | YYZ 6DoF 独立 initial/trajectory/termination | specification-only REF-YYZ manifest + design | `prepared_only` | authoritative source/assets + independent expected + target/Legacy mappings |
| `G1-X-004` | CAVH intermediate formula cross-check | specification-only manifest + formula design | `prepared_only` | authoritative edition/license + independent Eq17/Eq18/derivative/envelope/TDCT results |
| `G1-X-005` | Legacy facts Preserve/Fix/Delete | capture design uses Preserve/Fix/Retire；manifest has no disposition fields | `prepared_only` | fact-level approved classifications；formal Delete/Retire vocabulary mapping |
| `G1-X-006` | refs independent of node/provider/CSV order | designs explicitly require semantic identity independence | `prepared_only` | executable mutation proves independence，不能只靠声明 |
| `G1-X-007` | REF-YYZ source/PlanProof/journal/Observation/CSV/Diagnostic schema chain | one PlanProof example；REF-YYZ manifest only | `missing` | all artifacts machine-valid, hash-linked, expected/actual/replay complete |
| `G1-X-008` | terminology checker passes target blueprint/roadmap excluding raw expert input | accepted scoped report conformant, 276 terms/20 aliases/27 shared symbols; 15 baseline + 35 review-contract + 76 acceptance mutations | `verified_technical` | `closed` for the accepted source set；full-repository evolution coverage remains `R0-ARCH-002` |
| `G1-X-009` | all blocking gaps closed/rejected；unexplained science differences=0 | no gate difference ledger；source bundles absent | `missing` | complete difference ledger with every item classified/approved and unexplained=0 |
| `G1-X-010` | performance/determinism gate dependency | only design；no measurable target workload | `prepared_only` | PERF-001 done with approved profiles/raw samples/budgets/report |
| `G1-X-011` | provenance/license eligible | policy validator passes but ADR/license decisions pending | `review_pending` | GOV-002 done；each source/data/artifact eligible for intended use |
| `G1-X-012` | hosted/cross-tool evidence retained | toolchain、GOV-001 与 SPEC-001 已有 retained/commit-bound Hosted CI；其余科学任务尚无当前提交证据 | `review_pending` | SCI/Legacy/performance/gate 对应 runs、logs、artifacts 与 hashes 完整保留 |

### 7.1 Fixture/oracle inventory

| Bundle | Manifest status | Required gate role | Current verdict |
| --- | --- | --- | --- |
| `REF-SCIENTIFIC-CONVENTIONS-001` | `executable` | quaternion/frame/unit/time properties | technical pass, review pending |
| `REF-MINIMAL-3DOF-001` | `specification_only` | independent 3DoF initial/trajectory/termination | not executable |
| `REF-YYZ-001` | `specification_only` | YYZ 6DoF and source-to-evidence chain | not executable |
| `REF-CAVH-FORMULA-001` | `specification_only` | formula/intermediate/derivative/envelope/TDCT | not executable |
| `ORACLE-GNCZMKN-R0-001` | `planned` | 7 Legacy behavior facts | all 7 artifact refs empty |

只有 manifest status 为 `executable` 仍不自动意味着 gate pass；必须继续检查 source independence、expected/actual、tolerance、profile、difference 和 owner approval。

### 7.2 独立性规则

G1 reference 必须独立于被测路径的关键错误模式：

- minimal 3DoF：analytic solution 或独立工具/实现，不导入 product dynamics/integrator；
- YYZ：权威 mission/assets 与独立 formula/intermediate/trajectory lane，不能只复制 Legacy CSV；
- CAVH：独立 transcription/implementation 或可授权 paper table，不能让测试复制 production Eq17/Eq18 algebra；
- convention：C++ 与 Python lanes 不能共享 product quaternion implementation；
- Legacy：只证明旧行为事实与迁移对照，不自动成为“科学正确”；
- CSV：按 semantic FieldId/unit/frame/time 比较，不锁定列序、provider 名或 Node 数量。

只比较最终 aggregate/terminal value 会隐藏中间公式、phase、commit 或 timestamp 错误，属于 `invalid_for_gate`。

### 7.3 Scientific difference ledger

正式 G1 必须逐项记录：

| Field | Rule |
| --- | --- |
| `difference_id` | 稳定且可引用；不得用自由文本行号作 identity |
| `bundle/case/field/tick` | 精确定位 semantic comparand |
| `expected_ref` / `actual_ref` | 两者带 raw/canonical hash 与 producer |
| `comparator/tolerance` | identity/order/status 使用 exact；numeric 使用预先批准的 abs/rel/ULP/domain policy |
| `magnitude` | actual、absolute、relative、units；非数值差异记录 exact mismatch |
| `classification` | ScientificInvariant、DeclaredModelChoice、ImplementationDefect、AccidentalStructure、TargetOnlyContract 等批准枚举 |
| `legacy_disposition` | Preserve/Fix/Delete（若实施用 Retire，必须有 authority-approved mapping） |
| `owner/approval_ref` | 有权 Scientific/Architecture/Validation owner；Codex 不能代签 |
| `status` | explained/approved 或 unexplained；不得自动放宽 tolerance |
| `target_action` | preserve/fix/reject/defer 与关联 task/defect |

G1 hard rule：`count(status == unexplained) == 0`。waiver 不得把 unexplained 改名为 explained；只有证据和有权 classification 能关闭差异。

### 7.4 G1 blocker ledger

| Blocker | Owner | Close evidence |
| --- | --- | --- |
| `G1-BLK-001` convention ADR/review pending | Scientific Authority | ADR-0006/0007 + SCI-001 done |
| `G1-BLK-002` minimal 3DoF executable reference absent | Model SDK Lead + Scientific Authority | SCI-002 bundle/report |
| `G1-BLK-003` Legacy oracle artifacts/dispositions absent | Validation Lead + domain owners | LEG-002 executable suite |
| `G1-BLK-004` YYZ authoritative/independent bundle absent | Scientific Authority | SCI-003 source-to-evidence index |
| `G1-BLK-005` CAVH authoritative/independent bundle absent | Scientific Authority | SCI-004 formula bundle |
| `G1-BLK-006` performance/determinism baseline absent | Runtime Numerics Lead | PERF-001 report/raw/profile |
| `G1-BLK-007` difference ledger absent | Scientific Authority + Validation Lead | unexplained=0 signed ledger |
| `G1-BLK-008` provenance/license decisions pending | Product Owner | GOV-002 done + source eligibility |
| `G1-BLK-009` task-specific hosted evidence incomplete | Validation Lead | retained SCI/Legacy/performance/gate matrix runs |
| `G1-BLK-010` G0 not Passed | Architecture Lead + Product Owner | signed G0 Passed decision |

当前 G1 readiness：`not_ready_for_authorized_review`。

## 8. 正式 evidence index 设计

本阶段不新增公共 schema；以下是 gate review 必须物化的信息模型。若未来要成为 machine contract，先走 ADR 与 `R0-SPEC-001` 后继任务。

```text
GateEvidenceIndex
  identity
    gate_id
    index_id
    schema/version
    reviewed_commit
    generated_at
  authority_inputs[]
    role
    path/ref
    raw_hash
    status/decision_ref
  task_closure[]
    task_id
    status
    owner/assignee/reviewer
    implementation_commit
    acceptance_results[]
    evidence_refs[]
  criteria[]
    criterion_id
    authority_ref
    result
    evidence_status
    artifact_refs/hashes[]
    reviewer/approval_ref
    waiver_ref
  fixtures_oracles[]
    id/status/version
    source/input/reference hashes
    expected/actual/comparator/tolerance
    execution profile/run/report refs
  executions[]
    profile id
    OS/compiler/flags/fenv/runtime/binary hash
    command/run id/result
    raw artifact/report/index hashes
  scientific_differences[]
  waivers[]
  signatures[]
  decision
    result
    unlocked[]
    remains_locked[]
```

Index 本身也必须 canonicalize/hash，并引用 raw artifact；只保存报告摘要不足以重放。

## 9. G0/G1 正式评审流程

### 9.1 Freeze

1. 选择 reviewed commit，确认 remote/local 可获取；
2. clean checkout，submodule/dependency/tool versions 固定；
3. 捕获 authority input hashes、task/ADR/role snapshot；
4. 禁止评审期间静默修改 expected/tolerance/status；任何变化形成新 commit 并重启相应 evidence。

### 9.2 Dependency preflight

1. 9 个 direct dependencies 必须全部 `done`；
2. 每个 deliverable、acceptance、evidence 逐条读取，不能只看 status；
3. 验证 task assignee/reviewer/owner approval；
4. 检查 dependency commit 是 reviewed commit 的祖先；
5. 运行 backlog/schema/repository validator 和 gate mutations。

### 9.3 G0

1. 对 `G0-D-001`–`G0-D-014` 建 evidence rows；
2. 对 `G0-X-001`–`G0-X-008` 执行 positive/negative/withheld cases；
3. Architecture Lead 逐项 review；
4. 记录所有 missing/fail/waiver 与受影响 consumer；
5. Product Owner 在 Architecture Lead 签署基础上给出 G0 decision；
6. 只有 `Passed` 才可继续 G1 的解锁性 decision。

### 9.4 G1

1. 固定 approved convention、source、asset、formula、reference 与 tolerance versions；
2. 先执行 reference 自校验，再执行 candidate comparison；
3. 从 formula/property/component/mission/termination 多层比较，不能只看最终 CSV；
4. 执行 independence、order/provider/node/column mutations；
5. 生成 scientific difference ledger，逐项分类并审批；
6. 执行 determinism/performance/capacity profile，correctness fail 必须先阻断性能结论；
7. Scientific Authority、Validation Lead 和相关 domain owner 签署；
8. Product Owner 只能在 unexplained=0 且所有 hard criteria met 时给出解锁 R1 的 `Passed`。

### 9.5 Decision 与 release state

G0/G1 使用两份独立 records。每份至少包含：

- date/result/reviewed commit/current gate/unlocked gate；
- criteria + evidence hash + result；
- scientific differences；
- waivers；
- named reviewers/signatures；
- exact available/restricted capabilities。

R1 解锁必须是一个授权、可审查的原子变更：

1. 新增 signed G1 `Passed` record；
2. 更新 `R0-GATE-001` 状态/evidence；
3. 更新 `project-manifest.current_gate`；
4. 只把明确列出的 R1 tasks 从 `planned` 转到合法的 next state；
5. 保持未解锁 R1/R2+ tasks 不变；
6. 从 clean checkout 重新验证并提交同一 decision lineage。

任一文件单独更新、结果为 `Conditional`、未签署、unexplained>0 或 decision/manifest/backlog 相互矛盾时，R1 仍保持锁定。

## 10. Waiver policy

每个 waiver 必须包含：

- stable waiver id；
- gate/criterion/scope；
- missing contract/evidence；
- 受影响的 capability/consumer；
- risk 与最坏后果；
- named risk owner/approver；
- issue/task 与 closing evidence；
- issue date、expiry/date or event；
- result/unlock effect；
- review cadence。

以下不能被普通 waiver 变成 pass：

- G1 `unexplained scientific differences > 0`；
- 无权签署或 required role 缺失；
- reviewed commit/evidence hash 不明；
- `specification_only`/planned bundle 冒充 executable；
- provenance/license 明确禁止 intended use；
- `Conditional` 试图解锁消费缺失 contract 的阶段。

“以后补”或 backlog task id 本身不是 waiver；必须有具名风险接受和期限。

## 11. Gate decision matrix

| G0 | G1 | R1 state | 说明 |
| --- | --- | --- | --- |
| not reviewed / not ready | any | locked | 当前状态 |
| Failed | not applicable | locked | 修复后新 commit 重审 G0 |
| Conditional | not applicable for unlock | locked | 只允许不消费缺失 contract 的 R0 closure work |
| Passed | not ready/not reviewed | locked | 可进行 G1 closure/review，不可进入 R1 |
| Passed | Failed | locked | 科学缺口关闭后新 commit 重审 G1 |
| Passed | Conditional | locked | G1 Conditional 不解锁 R1 |
| Passed | Passed | exact approved R1 subset unlocked | 仍需 atomic state update 和 signed records |

## 12. Mutation suite

| Mutation | 假阳性 | 必须失败的检查 |
| --- | --- | --- |
| `GATE-MUT-001` | reviewed commit 缺失/不可获取/dirty drift | commit integrity |
| `GATE-MUT-002` | planned/review dependency 算作 done | dependency state |
| `GATE-MUT-003` | design/work package 算 executable artifact | evidence status |
| `GATE-MUT-004` | technical validator pass 算 owner approval | authority closure |
| `GATE-MUT-005` | Proposed ADR 算 Accepted | ADR status |
| `GATE-MUT-006` | null/placeholder/same-person reviewer 签署 | role completeness/independence |
| `GATE-MUT-007` | hosted CI pending 算 passed | run identity/evidence |
| `GATE-MUT-008` | specification-only fixture 算 executable | fixture status/artifact refs |
| `GATE-MUT-009` | planned oracle 无 artifacts 仍 pass | oracle completeness |
| `GATE-MUT-010` | Legacy hash/CSV 算独立 science truth | reference independence |
| `GATE-MUT-011` | unexplained difference > 0 仍 G1 Passed | scientific hard rule |
| `GATE-MUT-012` | report 无 raw/hash/profile/command | lineage/replay completeness |
| `GATE-MUT-013` | G0 未 Passed 先作解锁性 G1 | gate ordering |
| `GATE-MUT-014` | Conditional 解锁 R1 | unlock policy |
| `GATE-MUT-015` | waiver 缺 scope/owner/expiry/task/risk | waiver completeness |
| `GATE-MUT-016` | aggregate pass 掩盖 criterion fail | per-criterion completeness |
| `GATE-MUT-017` | profile/workload 不匹配的 perf 数字算 baseline | performance comparability |
| `GATE-MUT-018` | NOASSERTION/internal default 算 redistribution license | provenance eligibility |
| `GATE-MUT-019` | Codex/自审/无权人员代签 | signature authority |
| `GATE-MUT-020` | decision/backlog/manifest/unlock 非原子漂移 | release-state consistency |

未来实现应让同一 production readiness validator 接受正常包并拒绝全部 mutation；不能为负例写一套旁路检查器。

## 13. 关闭顺序

依赖闭合建议按 authority 和数据依赖，而不是按文档完成时间：

1. `[completed]` Product Owner 指派 required roles/reviewers；接受 ADR-0009，获得 Hosted CI evidence；
2. Product Owner/Architecture Lead 关闭 ADR-0008 与 intended-use/license/provenance decisions；
3. `[completed]` ADR-0004/SPEC-001 与 ADR-0005/ARCH-001 已完成 owner/reviewer 接受；
4. 实施并审查 ARCH-002，完成 G0 fixture/pressure/fitness evidence；
5. Scientific Authority 关闭 ADR-0006/0007 与 SCI-001 review；
6. 实施 LEG-002 和 SCI-002，建立最小独立基线；
7. 在已批准 source/provenance 上实施 SCI-003/SCI-004；
8. 以 executable SCI-002 workload 实施 PERF-001，分离 correctness/determinism/capacity/performance；
9. 生成 GateEvidenceIndex 与 difference/waiver ledgers；
10. 正式评审 G0；G0 Passed 后正式评审 G1；
11. 只有 signed G1 Passed 后原子解锁精确 R1 subset。

这不是对 backlog 的状态修改；各任务只有满足领取规则后才能从 `planned` 激活。

## 14. Readiness completion checklist

### Governance

- [x] 8 required roles 的 assignee/reviewer 完整且独立；
- [x] Product Owner、Architecture Lead、Scientific Authority 解析到已授权机器 actor；
- [ ] ADR-0004–ADR-0009 已有 authorized disposition；
- [x] 基础 correctness/conformance matrix 有 retained evidence；后续任务仍需各自 current-head evidence；
- [ ] intended-use/license/provenance eligibility 明确。

### Dependencies

- [ ] 9 个 direct dependencies 全为 `done`；
- [ ] 每个 deliverable/acceptance/evidence 可重放；
- [ ] indirect review tasks/ADRs 未被绕过；
- [ ] dependency commits 均包含于 reviewed commit；
- [ ] backlog、work package 与 report 无状态矛盾。

### G0

- [ ] `G0-D-001`–`G0-D-014` 全部有 evidence row；
- [ ] `G0-X-001`–`G0-X-008` positive/negative/withheld cases 完成；
- [ ] ChangeCard/CapabilitySlice/ChangeVector/route 资料 machine-valid；
- [ ] PlanProofRecord 七类 + PlanProofIndex + YYZ dry-run 完整；
- [ ] 13 pressure surfaces 与 representation/causal walkthroughs 闭合；
- [ ] deferred semantics 有 unsupported guard，无 callback/global hacks；
- [ ] architecture fitness suite 在 CI 证明正/负例。

### G1

- [ ] convention bundle/ADRs owner-approved；
- [ ] minimal 3DoF 独立 bundle executable；
- [ ] 7 Legacy oracle bundles executable，fact dispositions approved；
- [ ] REF-YYZ authoritative/independent source-to-evidence chain executable；
- [ ] CAVH independent formula bundle executable；
- [ ] node/provider/CSV order independence mutations pass；
- [ ] scientific difference ledger unexplained=0；
- [ ] PERF-001 approved workload/profile/raw/budget report complete；
- [ ] all artifacts/hash/lineage/replay commands complete。

### Decision/release

- [ ] reviewed commit clean/frozen；
- [ ] G0 record separately signed；
- [ ] G1 only after G0 Passed；
- [ ] waiver fields and unlock effects complete；
- [ ] G1 result exactly `Passed` before R1 unlock；
- [ ] decision/backlog/manifest/exact unlock atomically consistent；
- [ ] clean checkout verification retained。

## 15. 本审计的非声明

本文件不声明：

- G0/G1 已召开、失败、Conditional 或通过；
- 任一 `review` task 已被 owner 接受；
- 任一 `planned` task 已激活或实现；
- candidate toolchain 已 supported；
- hosted CI 已运行；
- repository 或 Legacy archive 可对外分发；
- minimal 3DoF、YYZ、CAVH 有可执行 reference；
- Legacy 是独立科学 truth；
- D0–D3、容量、性能或实时指标已达成；
- R1 或任何 R1–R8 production capability 已解锁。

## 16. 审计结论

在 audited commit 上：

- `R0-GATE-001` activation：不合格；
- G0 authorized review readiness：不合格；
- G1 authorized review readiness：不合格；
- G0 official result：不存在；
- G1 official result：不存在；
- R1 unlock：禁止；
- 允许的下一步：关闭 R0 direct/indirect dependencies、完成具名 governance decisions、实现设计好的 R0 guards/oracles/scientific/performance evidence，再按本文件程序重新生成 readiness snapshot。

该结论来自可观察状态而非主观成熟度评分。任何后续 readiness 变化必须由新的 committed evidence、任务状态、authorized decisions 和可重放验证共同支持。
