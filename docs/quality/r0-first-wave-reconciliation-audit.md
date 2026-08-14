# R0 remote first-wave reconciliation audit

- Audit id：`R0-FIRST-WAVE-RECON-20260810-001`
- Current line commit：`08400c3de1c6187439a867e0a3b980f935dbe7a3`
- Current line tree：`1e54cd9beb4c4e33ffce1086ed2ebc37a8f9e4ad`
- Candidate remote ref：`origin/codex/r0-first-wave`
- Candidate commit：`7d7c0a8f1ec52517dfccb3b7c9b386da5e70f74b`
- Candidate tree：`b4d4e0b8d2f19909adf1d9c6bc64e19d12a61a69`
- Common ancestor：`7b05c29f5863d6230fad9285371dc98bd2a30c0d`
- Prepared by：Codex
- Prepared on：2026-08-10
- Disposition：`selective_reimplementation_required`
- Reconciled through：`7d43a64d15590dead4082939d4188ec8b947133b`
- Reconciliation status：`technical_slices_committed_authority_decisions_open`

## 1. 结论

`origin/codex/r0-first-wave` 是一条从 B0 单独分叉的 R0 实现线，不是当前线的上游。当前线相对共同祖先有 13 个 commits，candidate 线有 1 个 commit；当前线修改 138 个 tracked paths，candidate 修改 157 个，只有 17 个 paths 重叠。candidate commit 一次性增加约 25,809 行，覆盖治理、schema、架构、Legacy reproduction、科学约定和 provenance。

candidate 在隔离 worktree 上可构建且自洽：MSVC Debug 5/5 CTest 通过，repository verification 验证 94 个 JSON、65 个任务和 89 个 Markdown；其 schema suite 接受 6 个正例并拒绝 52 个要求的反例与 1 个非法 schema。GitHub Actions run `31351615598` 也在 candidate commit 上显示 `windows-latest` 与 `ubuntu-latest` 两个 jobs 成功。

这些结果证明 candidate 有可复用的技术资产，但不能整分支 merge/cherry-pick：

1. candidate 用 `codex-r0-*` 虚拟 execution seats 填充 accountable roles，并由这些身份互相“批准”全部新增 ADR、把 6 个 R0 tasks 标为 `done`；当前治理要求 accountable identities、禁止 placeholder，且 Codex 不能替代真实 Product Owner、Architecture Lead 或 Scientific Authority；
2. candidate 的 ADR-0004–ADR-0011 与当前 ADR-0004–ADR-0009 发生编号和主题冲突，且 candidate 的 `Accepted` 状态依赖上述无效签署；
3. 三份 R0 schema 的 `$id`、字段形状和引用模型与当前 contract 不兼容，candidate 还把 4 份未闭合 manifest 降为 `placeholder/0` 以绕开 v1 validation；
4. candidate quaternion fixture 把 passive `+theta` axis-angle 编码成 `[cos(theta/2), -u sin(theta/2)]`，而当前候选 ADR/fixture 明确用正 z coefficient 得到 `x -> -y`；两者虽都使用 `q^-1 v q`，但对 axis-angle 正方向的外部语义不同，不能混用 expected values；
5. candidate Legacy evidence 只覆盖 2 个代表任务、包含大量绝对 host paths/raw build logs，且记录 `baseline_matched: false`；当前线已有 27/27 tests、5 missions 双跑和结构化 evidence index，覆盖更完整；
6. candidate hosted CI 使用 rolling `windows-latest`/`ubuntu-latest` 和 `actions/checkout@v4`，并出现 Node 20 deprecation warnings；该 run 只证明 candidate commit，不能转移为当前 fixed candidate profiles 的资格证据。

因此采用策略是：保留 candidate commit 为 immutable comparison source；逐域提取 test idea、failure mutation 和缺口；在当前 authority、schema、naming、provenance 与 gate 规则下重新实现；每个提取切片单独审查和提交。candidate 的 role、ADR status、task status、expected scientific values、placeholder manifests 和 release state 一律不迁移。

## 2. 审计方法

### 2.1 Evidence classes

| Class | 含义 | 本审计如何处理 |
| --- | --- | --- |
| `adopt` | bytes 与当前 authority/contract 一致，可保留来源后直接引入 | 当前没有整文件满足此条件 |
| `adapt` | 技术思想或测试有价值，但必须映射到当前 contract 后重写 | schema negative cases、science edge properties、provenance mutations、logical boundaries |
| `evidence_only` | 只证明 candidate commit/environment，不可成为当前 truth | candidate local/hosted runs、Legacy captures、cross-tool report |
| `superseded` | 当前线有更完整、可重放或治理更严的实现 | Legacy reproduction、role schema/policy、derived architecture validation 部分 |
| `reject` | 会引入越权、语义冲突、状态漂移或不可追溯数据 | virtual approvals、done states、ADR numbering/status、placeholder downgrade、axis-angle expected values |
| `decision_required` | 两条线提出不同公共语义，必须由有权 owner 选择 | schema shape/ref model、axis-angle sign、logical-boundary authority、scientific provenance contract |

### 2.2 Validation boundary

candidate 的 verifier pass 只证明它对自身 schema、tests 和 recorded expectations 自洽。本审计另行检查：

- 与蓝图、accepted ADR、当前 Proposed ADR 和 role policy 的兼容性；
- 测试是否独立于被测实现，expected 是否来自同一语义；
- task/ADR approval 是否由有权身份作出；
- schema/manifest 是否通过降级或旁路制造 pass；
- evidence 是否绑定 commit、profile、raw artifacts、hash 与 lineage；
- 是否能在当前线不改变公共语义地移植。

## 3. Source anchors

### 3.1 Candidate raw SHA-256

| Candidate path | SHA-256 |
| --- | --- |
| `docs/team/role-assignments.json` | `8624fece1181931f24dd77435946cd40bd6a94f784828b6dfb2fe8ecc6ecde98` |
| `docs/tasks/backlog.json` | `17e04d8f88543fb4df0e97f0ac90c7df9f64a08983bd818374dd3d7674a73c1e` |
| `specs/fixture-manifest.schema.json` | `2bf5b4f738c80d80432be8137a4c8e7b1087945ca0ecd07c10687078a79ff1cd` |
| `specs/oracle-manifest.schema.json` | `fb143065abe310ece70191bf54ae5d2ba99ee95520654ee2754cab5a822d7a80` |
| `specs/plan-proof-record.schema.json` | `561e035252b1f9c49bfba480d53f98f9913f30db10609a603db8c09601e93fea` |
| `tools/r0_spec_schema_validator.py` | `60ad5fc78443ed99f640ef9af860a71e2380ef681aa506283745c7226e653075` |
| `tests/r0-spec-001/cases.json` | `bae1ef8db7018ba3c858e5e1c07446a5abd5757f2f701cecfbbf834568471566` |
| `tests/scientific_conventions_properties.cpp` | `0ff18ce21b4943a5eb0fa09be5afd8be52e9fe9b3eaef630467fc8056a9f603f` |
| `oracles/scientific-conventions/reference.py` | `7f0ac6ed9682854cce12c8b04381aed2c9f82d633884e0fdf047cb261259e05b` |
| `oracles/scientific-conventions/evidence/cross-tool-report.json` | `e6866731a01524005d14803505b21d7d2eb487e92ee7c77e7f3563da4f2281dc` |
| `specs/architecture/r0/terminology-baseline.json` | `6d2731dabf816993f5430839d37f91e96f528ba476a7d63fa7a275a7eeadfeb1` |
| `specs/architecture/r0/module-dependency-map.json` | `68e400fc7735123f2b84ecbeaa88a639a127da059d009586111bd1a2e9d4d92c` |
| `specs/architecture/r0/legacy-to-target-ownership-map.json` | `876aee402ea772e89e25280485c6f846c7c45c938d86427626911e91b4d5d967` |
| `docs/quality/provenance-register.json` | `cd79f0d04a3b2c0fdca339548f402f8ffcbd29c2be214d451f1df8bc48c9ef38` |
| `docs/quality/r0-leg-001/runs/20260809T081000Z/result-summary.json` | `253cdecfd5dffb655ebc07fd4cf28f8fb9a2cda081d8ed4f6778ddf589fde949` |

### 3.2 Hosted evidence

- Workflow run：`https://github.com/aofenghanyue/GNCSIMZMKN/actions/runs/31351615598`
- Commit：`7d7c0a8f1ec52517dfccb3b7c9b386da5e70f74b`
- Trigger：push，2026-08-10 03:05 UTC；
- Result：Success，2 jobs；
- Ubuntu job：16 s；
- Windows job：32 s；
- Warning：两个 jobs 均报告 `actions/checkout@v4` 的 Node.js 20 deprecation；
- Artifacts：run summary 显示 `–`；
- Transferability：仅 `evidence_only`，不是当前 commit/profile 的 hosted pass。

## 4. Branch topology and overlap

| Fact | Value |
| --- | --- |
| Common ancestor | `7b05c29` |
| Current-only commits | 13 |
| Candidate-only commits | 1 |
| Current changed paths vs B0 | 138 |
| Candidate changed paths vs B0 | 157 |
| Overlapping changed paths | 17 |

重叠 paths：

```text
.gitignore
CMakeLists.txt
LICENSE-STATUS.md
docs/adr/README.md
docs/tasks/backlog.json
docs/team/role-assignments.json
fixtures/README.md
fixtures/ref-cavh-formula/fixture-manifest.json
fixtures/ref-minimal-3dof/fixture-manifest.json
fixtures/ref-yyz-001/fixture-manifest.json
oracles/README.md
oracles/oracle-manifest.json
specs/README.md
specs/fixture-manifest.schema.json
specs/oracle-manifest.schema.json
specs/plan-proof-record.schema.json
tools/verify-repository.ps1
```

这些恰好是状态、authority、schema 与 gate 最敏感的 files；用 merge conflict resolution 拼接会掩盖语义选择，必须逐项 reimplement。

## 5. Isolated reproduction

### 5.1 Local candidate run

隔离 worktree：detached `7d7c0a8`，未修改 tracked files。

环境：

- Windows 11 x64 build 26200；
- MSVC 19.50.35725；
- CMake 4.1.2；
- Ninja 1.12.1；
- CPython 3.13.5；
- preset `dev` / Debug。

结果：

| Check | Result |
| --- | --- |
| Configure/generate | passed |
| Build | passed |
| CTest | 5/5 passed |
| JSON parse inventory | 94 passed |
| Backlog entries | 65 passed |
| Markdown links/files | 89 passed |
| Provenance positive/negative suite | passed |
| Architecture guard sentinels | passed |
| R0 schema conformance | 6 valid accepted；52 required invalid rejected；1 invalid schema rejected |

### 5.2 What this run does not prove

- virtual seat identities are accountable people；
- ADR/task `Accepted`/`done` transitions are authorized；
- candidate schema is compatible with current schema；
- candidate quaternion expected values match current convention；
- candidate evidence can be distributed；
- rolling hosted environments are fixed profiles；
- candidate commit closes current G0/G1；
- current line inherits the candidate hosted result。

## 6. Governance and task-state audit

### 6.1 Candidate roles

Candidate schema v1 assigns:

- `product_owner -> codex-r0-coordinator`；
- `scientific_authority -> codex-r0-science`；
- `architecture_lead -> codex-r0-architecture`；
- `validation_lead -> codex-r0-validation`；
- remaining roles reuse those virtual seats；
- reviewers differ by string, but no human identity/authorization evidence exists。

Current role schema v2 requires non-placeholder identities, assignee/reviewer separation and Science/Architecture assignee independence. Current validator contains a mutation named `virtual-role-readiness`, but that mutation only proves an incomplete role file cannot be made ready by changing `decision_status`; its exact placeholder regex rejects `Codex` but does **not** yet reject `codex-r0-*`. This audit therefore exposes a current-validator gap. A candidate label beginning `codex-r0-*` is a process seat with no human authorization evidence, not an accountable person. Candidate approvals remain `invalid_for_gate`, and the current validator needs an explicit virtual-seat mutation before it can enforce that judgment automatically.

### 6.2 Candidate task states

Candidate changes these R0 tasks to `done`：

- `R0-GOV-001`；
- `R0-GOV-002`；
- `R0-ARCH-001`；
- `R0-LEG-001`；
- `R0-SCI-001`；
- `R0-SPEC-001`。

The states rely on virtual-seat review text embedded in evidence files. They cannot overwrite current `review` states. Technical artifacts may support a future human review, but status must be recomputed against current deliverables/acceptance/evidence.

### 6.3 ADR collision

| Number | Current topic/status | Candidate topic/status | Disposition |
| --- | --- | --- | --- |
| 0004 | R0 JSON schemas / Proposed | license/provenance / Accepted | collision；candidate approval rejected |
| 0005 | derived architecture baseline / Proposed | SI/numeric / Accepted | collision；candidate approval rejected |
| 0006 | SI/frame/time / Proposed | frame transform / Accepted | collision；candidate content compare only |
| 0007 | passive Hamilton quaternion / Proposed | time values / Accepted | collision；candidate content compare only |
| 0008 | internal license/provenance / Proposed | quaternion / Accepted | collision + science semantic difference |
| 0009 | roles/candidate toolchain / Proposed | R0 schemas / Accepted | collision + incompatible schema |
| 0010 | absent | architecture baseline format / Accepted | decision candidate；requires current ADR allocation/owner |
| 0011 | absent | scientific terminology overlay / Accepted | decision candidate；requires provenance/owner review |

No candidate ADR bytes or status is copied. Useful rationale is routed into current Proposed ADR review findings.

## 7. Schema and validator audit

### 7.1 Incompatible contracts

| Contract | Current | Candidate | Consequence |
| --- | --- | --- | --- |
| `$id` | `https://internal.gnczmkn/schemas/.../1` | `urn:gnczmkn:schema:r0:...:1` | same version label, different identity |
| Fixture provenance | nested `provenance.source_refs` | `provenance_refs` + `source_refs` | incompatible field graph |
| Fixture oracle relation | not present | required `oracle_refs` | candidate instances cannot validate current schema |
| Expected facts | object `{id, claim, tolerance_policy, evidence_refs?}` | candidate-specific fact objects/comparison model | incompatible comparison semantics |
| Oracle facts | strings per oracle | typed fact records in candidate | incompatible migration |
| Proof premises | object map | candidate typed premise graph | proof reference/cycle features cannot be copied alone |
| Existing incomplete bundles | valid v1 + explicit status | candidate `placeholder/0` bypass | candidate does not validate four actual bundles as v1 |

### 7.2 Candidate strengths to adapt

Candidate suite exposes useful failure classes not fully covered by current examples：

- duplicate JSON object keys；
- `NaN`/non-finite JSON number；
- repository path traversal、POSIX/Windows absolute path、drive-relative and file URI；
- stable id malformed/duplicate across validation set；
- authority id not in role registry；
- open task missing or already done；
- unresolved local artifact/provenance/oracle refs；
- proof prerequisite self-reference、unresolved ref、cycle、duplicate premise；
- result-specific diagnostic/operator cardinality；
- unknown schema keyword/property/version/enum。

### 7.3 Porting boundary

The first amendment may add only checks whose semantics already follow current contracts and accepted governance：

- strict JSON lexical rejection for duplicate keys and non-standard non-finite tokens；
- global fixture/oracle/proof identity uniqueness within the validation set；
- fixture `authority` resolution against current role registry；
- fixture `open_tasks` resolution against current backlog and non-`done` status；
- executable/qualified bundle artifact completeness when current fields can express it；
- extra result-specific PlanProof negative examples already defined by current schema。

Path/ref resolution、typed proof prerequisite graph、oracle fact model 与 schema `$id` changes require ADR-0004 owner decision；2026-08-12 的初始实现已形成 `RECON-DEC-001`～`003` proposed disposition，并继续等待独立 commit-bound review。

## 8. Architecture audit

### 8.1 Coverage comparison

| Metric | Current derived baseline | Candidate split baseline |
| --- | --- | --- |
| Canonical terms | 276 | 309 |
| Aliases | 20 | 20 |
| Shared enum/key/owner symbols | 27 combined | 13 enums + 4 keys + 4 domains |
| Physical CMake modules | 9 | 9 |
| Logical boundaries | blueprint narrative | `packages_user` + `composition_root` |
| Legacy migrations/mappings | 22 | 23 registered + 4 audit-only |
| Negative cases | 9 | 10 |
| Representation | single deterministically derived artifact | 3 manually versioned split artifacts |

Raw count differences are not pass/fail. Candidate adds a nine-term scientific overlay and treats two logical boundaries as module-map entries; current derives vocabulary directly from the glossary and accepted ADR-0003. Candidate also contains four audit-only Legacy concepts absent from the current ownership list.

### 8.2 Adapt candidates

- compare all 33 candidate target responsibilities with current 22 migrations and classify true gaps vs different granularity；
- represent `packages/user contribution` and `composition root` as logical boundary coverage without violating ADR-0003's nine-module physical DAG；
- add candidate negative ideas：authoritative source drift、scientific overlay drift、missing Legacy coverage；
- keep current deterministic generator/source hashes；do not copy candidate accepted overlay as authority；
- route any new canonical term through glossary/ADR owner rather than inventing local overlay truth。

### 8.3 Responsibility reconciliation result

所有 27 个 candidate mapping identities 和 33 项 responsibility 已逐项与当前派生 baseline 对照。candidate 所有 `target_terms` 都能解析到当前 276 个 canonical terms；差异不在术语缺失，而在 mapping identity、owner 粒度与逻辑边界的 authority。

| Classification | Count | Responsibility ids | Disposition |
| --- | ---: | --- | --- |
| current owner/consumer aligned | 22 | `assembly-context-lowering`、`auto-data-recording`、`config-manager-compile`、`config-node-source`、`config-reader-frontend`、`discrete-node-descriptor`、`execution-phase-transaction`、`framework-catalog-view`、`discrete-task-obligation`、`observable-projection-plan`、`record-sink-port`、`mission-assembler-passes`、`node-registry-plan-handles`、`node-registry-session-state`、`observable-field-contract`、`onboard-state-contracts`、`simflow-experiment`、`simulation-builder-operation`、`simulation-node-definition`、`simulator-session`、`simulator-control`、`simulator-comparison` | covered by current primary/secondary ownership；candidate term detail remains comparison evidence |
| proposed physical owner split | 3 | `execution-phase-plan` → compiler、`onboard-state-input-view` → compiler、`simulation-builder-plan` → compiler | do not import；Architecture Lead decides whether current ownership needs a reviewed split |
| logical contribution routing | 2 | `guidance-process-recipe`、`node-factory-contribution` → `packages_user` | retain as ARCH-002 source-boundary design；not an ADR-0003 physical module |
| absent current mapping identity | 6 | `continuous-group-plan`；`integrator-plan`、`integrator-outcome`；`summary-observer-metrics`；`math-types-contract-boundaries`；`simulation-summary-workflow` | `IContinuousGroup` is candidate-registered；the other four names are candidate `audit-only`；none has a current glossary migration row |

The partition is closed：`22 + 3 + 2 + 6 = 33`。The 27 names are the current 22 plus `IContinuousGroup`、`IIntegrator`、`ISummaryObserver`、`math_types.hpp` and `SimulationSummary`。No current ownership row、glossary row or canonical term was changed。

### 8.4 Safe hardening port

The current deterministic validator now requires exact property sets for the authority registry and every module、shared-symbol and Legacy-ownership row。Six added mutation groups reject：

1. an unreviewed top-level `logical_boundaries` extension；
2. an unreviewed module-row `kind` extension；
3. an unreviewed shared-symbol `owner_role` extension；
4. a Legacy `responsibilities` overlay；
5. physical-module promotion of both `packages_user` and `composition_root`；
6. ownership rows for all five candidate-only names without glossary migration rows。

Together with the original nine cases, 15/15 invalid mutations are rejected。The source-hash case now writes mutated derived bytes through the same strict generated-content evaluator；it no longer relies on a string inequality assertion。The baseline remains a projection of the same glossary、ADR-0003、physical partition、authority registry and CMake graph：276 terms、20 aliases、27 shared symbols、22 Legacy mappings、9 physical modules and 22 CMake edges。

2026-08-12 接受基线增加 `r0-architecture-review-contract/1`：`RECON-DEC-006` 接受 `logical-only-keep-current`，`packages_user` / `composition_root` 只登记为逻辑规则标签；`RECON-DEC-007` 接受 `keep-current-22-owner-consumer-map`，33 项 responsibility 继续按 `22 + 3 + 2 + 6` 分类。contract 锁定 11 个 authority/generator/derived entries、2 个 logical boundaries、零 runtime consumer 与 35 项新增 mutation。capability section/header/identity/commitment/gate、合法 ownership/source-root 漂移、隐藏 CMake/ADR dependency、duplicate JSON key、BOM/CRLF derived bytes 与治理 artifact runtime consumption 均进入真实失败路径。Architecture Lead 与独立 Validation Lead 已接受技术提交 `29f455efebd72113c1d311bc674a78c638265f34` 及 15 路径文件集 SHA-256 `16d566512cd3d0bdb8e4f9fc84f3c8709328708aba7b4f95b4570b8a3f6a9561`；接受 guard 另行拒绝 76/76 项 commit、actor、decision、contract、evidence、CI、状态文档与 boundary mutation。ADR-0005、两项 decision 和 R0-ARCH-001 已形成原子接受记录。

## 9. Scientific convention audit

### 9.1 Coverage comparison

| Aspect | Current | Candidate |
| --- | --- | --- |
| C++ summary | 23 semantic check categories；1819 assertions；256 randomized rotation samples | 559 individual checks；128 randomized samples |
| Python summary | same 23 categories/1819 assertions/16 observations | 554 checks；6 cross-tool cases |
| Cross-tool | 48 numeric values；0 mismatches | 102 fixed values；0 mismatches |
| Direction fixture | positive z coefficient maps x to `-y` | negative z coefficient labelled passive +90 maps x to `+y` |
| Extra candidate edges | independently covers malformed/nonfinite/non-unit quaternion、NormalizeWithFlag、invalid units/Kelvin、clock domains and Proposed half-open validity | candidate supplied the comparison ideas |

### 9.2 Critical scientific difference

Both lines agree on Hamilton multiplication, `q_to_from`, `v_to = inverse(q) * v_from * q`, right-multiplication composition and `[w,x,y,z]`. They differ in what coefficients represent a positive axis-angle input：

- current ADR-0007 candidate：`[sqrt(1/2),0,0,+sqrt(1/2)]` maps from-frame x to to-frame `-y`；
- candidate ADR-0008：a desired passive `+theta` matrix uses `[cos(theta/2),-u sin(theta/2)]`，so its z-quarter quaternion has negative z and maps x to `+y`。

The blueprint fixes transform algebra but does not explicitly define the external axis-angle positive-sign adapter. This is an `unexplained/decision_required` representation difference, not a numerical error. Candidate fixed values and report cannot be mixed with current fixture until Scientific Authority approves one labelled axis-angle mapping or explicitly supports both as differently named adapters.

### 9.3 Safe property ports

After preserving current direction expected values, the following candidate tests were evaluated for independent reimplementation：

- exact four finite quaternion coefficients；
- zero norm、non-unit Reject、NormalizeWithFlag and correction flag；
- `q/-q` rotation equivalence and malformed metadata；
- reflection/non-orthogonal matrix rejection；
- unknown/non-finite unit rejection and below-zero Kelvin rejection；
- same-clock time arithmetic、mixed-clock rejection；
- half-open validity interval、reversed interval and non-finite time rejection。

Slice B implements the coefficient/normalization、unit-domain、clock-domain and validity items in both isolated lanes and reflects them in fixture/report hashes. Existing `q/-q` coverage remains；matrix/metadata rejection stays outside this amendment. Candidate code and its conflicting axis-angle helper were not copied.

## 10. Legacy evidence audit

| Aspect | Current | Candidate | Disposition |
| --- | --- | --- | --- |
| Clean CTest | 27/27 | 25/25 | current supersedes |
| Historical path issue | 23/25 + direct rerun recorded | same discrepancy recorded | equivalent fact |
| Representative missions | 5, each double-run | 2, single captured run | current stronger |
| Determinism | per artifact double-run hash | clean extraction hashes, not full double-run mission evidence | current stronger |
| Evidence index | structured 31-file index with hashes | hash file + large logs | current stronger |
| Host paths | normalized semantic paths in reports；logs evidence-only | absolute user/worktree paths in JSON and logs | candidate not portable |
| Candidate status | current `passed` with explicit scope | `complete_with_documented_gaps` and `baseline_matched=false` | candidate evidence-only |

No candidate Legacy log/CSV is imported. Its run remains reachable by commit hash for historical comparison. Useful new bytes would require provenance review and deduplication against the current frozen archive/evidence index.

## 11. Provenance audit

Candidate's provenance model has useful structural ideas：

- closed subject types；
- explicit scientific context；
- independent-reference assertion and basis；
- generated artifact upstream refs；
- restriction inheritance；
- classification cannot become weaker than inputs；
- `NOASSERTION` cannot authorize external sharing。

Current line already fails closed on missing provenance, license text, `NOASSERTION` distribution, Legacy hash drift, unauthorized license conclusion and checkout pin drift. Candidate adds stronger generated-lineage/scientific-context mutations. Those ideas are `adapt` candidates for R0-GOV-002 review, but candidate register truth and approval fields are rejected because they rely on virtual seats and different policy schema.

No candidate record may change current facts：repository distribution license remains unselected；Legacy/source rights remain unresolved；external sharing remains blocked。

### 11.1 Current-model mapping result

The candidate cases divide into two groups：

| Candidate idea | Current representation | Slice D disposition |
| --- | --- | --- |
| closed subject types | 8 current inventory `category` values plus fixed category/classification pairs | enforce without adding a schema field |
| generated upstream refs | non-empty、unique、resolving、acyclic `lineage_parents` | enforce presence/resolution for the generated-evidence category |
| generated integrity | existing `integrity.kind` plus value/SHA/byte identity | close the six current integrity kinds and reject empty identity |
| restriction inheritance | `external_distribution` remains `blocked-*` for tracked generated evidence while upstream rights are unresolved | enforce the current fail-closed state；do not invent a clearance rank |
| weaker generated classification | category/classification mismatch | reject known invalid pair；do not claim a general cross-domain classification ordering |
| scientific context and independent-reference assertion | no current per-artifact field；`r0-research-evidence` aggregates scientific、Legacy-derived and governance evidence | do not import；requires `RECON-DEC-005` and Scientific Authority review |

The current inventory bytes and all eight `NOASSERTION` conclusions remain unchanged。No candidate provenance record、reviewer、approval、scientific claim、rights basis or external-sharing state is copied。

Verification passed under Windows PowerShell 5.1 and MSVC x64：Debug 9/9 CTest、Release 9/9 CTest、repository verification（56 JSON、65 task entries、98 Markdown）and `git diff --check`。All 14 mutations matched their intended diagnostics。The conformance report raw SHA-256 is `d47def50dd372ddf68a334917da4b93f070c582beac07b50db82e9abb97a1284`。

## 12. CI audit

Candidate hosted run is authentic execution evidence for commit `7d7c0a8`：two jobs succeeded. It does not satisfy current GOV-001 because：

- reviewed commit differs；
- workflow bytes/profile differ；
- candidate uses rolling `*-latest` labels rather than current fixed families；
- checkout action is `@v4` and emitted Node deprecation warnings, while current matrix pins an exact checkout commit；
- no retained artifacts are listed；
- current branch has not been pushed/run。

The URL and result should be retained in the reconciliation evidence index. Current hosted status remains `pending-push-and-run` until a current commit is pushed and both approved jobs produce retained identity evidence.

2026-08-12 supersession note：the current branch later satisfied that condition. Push run `31559701566` tested `32f6ade7f2feec6fb1792121773d437f6f035581`; PR run `31559704268` tested merge-context commit `4ecb1cafc8fab2dc385d1fb5493aa823b48b08bd`. The accepted [Hosted CI evidence receipt](hosted-ci-evidence-R0-GOV-001.json) permanently retains exact identities, required-step conclusions, downloaded log hashes and the 90-day upstream retention setting.

## 13. Selective implementation plan

### Slice 0 — accountable role identity hardening（implemented）

Scope：`R0-GOV-001` review amendment。

1. placeholder identity matching rejects known `codex-*` process seats, not only the exact string `Codex`；
2. `distinct-codex-seat-assignments` fills every role with distinct `codex-r0-*` assignee/reviewer strings and forces apparent completion；
3. the production validator rejects the mutation and reports every virtual identity；
4. real role assignments remain null；no human identity is selected；
5. readiness report、Debug/Release and repository verification are regenerated/reviewed in the amendment commit。

### Slice A — strict R0 contract parsing and registry semantics（implemented）

Scope：`R0-SPEC-001` review amendment。

1. dependency-free strict JSON parsing rejects decoded duplicate keys plus `NaN`、`Infinity` and `-Infinity` before `ConvertFrom-Json`；
2. fixture authority resolves to the current role registry；open tasks resolve to non-`done` backlog entries；
3. actual manifest registry validates 25 fixture/fact/oracle-set/oracle identities；5 mutations cover cross-file fixture/fact/oracle/proof duplicates；
4. executable/qualified facts and oracles require repository-resolving evidence/artifact files under the current evidence-location convention；this does not close `RECON-DEC-002` public reference grammar；
5. current-schema-shaped matrix passes 5 actual manifests、6 valid、16 targeted invalid and 9 validator failure cases；every invalid case has an expected diagnostic；
6. all three public schema bytes、versions and field graphs remain unchanged；no placeholder downgrade or typed-premise graph is imported；
7. Windows PowerShell 5.1 targeted validation、Debug/Release 9/9 CTest、repository verification（56 JSON、65 tasks、98 Markdown）and diff check pass；PowerShell 7/current hosted evidence remains pending push/run；
8. commit independently。

### Slice B — scientific edge-property strengthening（implemented）

Scope：`R0-SCI-001` review amendment。

1. exact-four/finite quaternion coefficient checks reject malformed inputs；`Error` rejects non-unit input while `NormalizeWithFlag` corrects it and exposes `normalized=true`；
2. unit boundary checks accept absolute zero and reject unknown/nonfinite/overflow/below-zero-Kelvin inputs；
3. distinct fixture-only time types enforce finite values、non-empty/same clock domains and half-open validity, while documenting half-open semantics as Proposed pending owner acceptance；
4. C++ and Python independently execute 23 checks/1819 assertions；16 observations/48 values compare with 0 mismatch and 0 maximum cross-tool difference；
5. report validation requires positive assertion counts and rejects 6/6 mutations, including an unexecuted check falsely marked pass；
6. current positive-z quaternion direction fixture and all 16 expected observations remain unchanged；axis-angle sign remains `RECON-DEC-004`；
7. MSVC Debug/Release 9/9 CTest and repository verification pass；report raw SHA-256 is `c121c74b546b2ad7722a6a5d90ee8ca0de028c4524fce624a3b95963138252c8`；
8. commit independently。

### Slice C — architecture delta reconciliation（implemented）

Scope：ARCH-001 review / ARCH-002 preparation。

1. all 27 mappings / 33 responsibilities are partitioned as 22 aligned、3 owner-split proposals、2 logical-boundary routes and 6 responsibilities on 5 absent names；all target terms resolve in the current glossary；
2. `packages_user` / `composition_root` remain logical rule labels, not new physical modules；
3. exact registry shapes and 6 mutation groups were ported into the deterministic validator, raising the suite from 9 to 15 without changing authority content；
4. ARCH-002 preparation now carries ARCH-MUT-015/016 and remains `planned` / unassigned；
5. Architecture Lead / Validation Lead decisions remain open for representation、mapping identity and owner granularity；
6. Windows PowerShell 5.1 targeted validation、MSVC Debug/Release 9/9、repository verification and diff review passed；commit independently。

### Slice D — provenance mutation strengthening（implemented）

Scope：GOV-002 review amendment。

1. map candidate closed types、generated upstream/integrity、restriction and classification ideas onto the existing governance-only inventory without changing its bytes；
2. require exact current category/classification and integrity kinds；generated evidence must have resolving lineage and remain externally blocked；
3. raise the suite from 8 to 14 mutations and require every mutation to match its intended diagnostic；
4. keep `scientific_context` / independence out of the aggregate model pending `RECON-DEC-005`；
5. keep all 8 license conclusions at `NOASSERTION`、repository license unselected and external sharing blocked；
6. Windows PowerShell 5.1 targeted validation、MSVC Debug/Release 9/9、repository verification and diff review passed；commit independently。

### Explicit non-slices

- no whole-commit cherry-pick/merge；
- no virtual roles or approval text；
- no candidate backlog status；
- no candidate ADR number/status；
- no placeholder/0 manifest downgrade；
- no candidate axis-angle expected values；
- no duplicate Legacy logs/CSVs；
- no transfer of hosted success to current commit。

## 14. Decision ledger

| Decision id | Required decision | Owner | Current disposition |
| --- | --- | --- | --- |
| `RECON-DEC-001` | current schema field graph/$id vs candidate graph；migration/version policy | Architecture Lead + Validation Lead | `keep-current` / accepted；[record](../governance/reconciliation-dispositions/RECON-DEC-001-2026-08-12.json) |
| `RECON-DEC-002` | repository/local/external reference resolution grammar | Architecture Lead + Evidence Lead | `repository-root-only` / accepted；tracked nonempty regular Git blob；[record](../governance/reconciliation-dispositions/RECON-DEC-002-2026-08-12.json) |
| `RECON-DEC-003` | PlanProof typed prerequisite graph/index boundary | Architecture Lead + Compiler Lead | `keep-current` / accepted；typed graph moves to v2；[record](../governance/reconciliation-dispositions/RECON-DEC-003-2026-08-12.json) |
| `RECON-DEC-004` | axis-angle positive sign and adapter naming | Scientific Authority | open |
| `RECON-DEC-005` | scientific context/independent-reference provenance contract | Scientific Authority + Product Owner | open |
| `RECON-DEC-006` | `packages_user`/`composition_root` representation and source authority | Architecture Lead | `logical-only-keep-current` / accepted；[record](../governance/reconciliation-dispositions/RECON-DEC-006-2026-08-12.json) |
| `RECON-DEC-007` | candidate extra Legacy mappings/responsibility granularity | Architecture Lead + Validation Lead | `keep-current-22-owner-consumer-map` / accepted；[record](../governance/reconciliation-dispositions/RECON-DEC-007-2026-08-12.json) |
| `RECON-DEC-008` | current hosted runner/action profile and artifact retention | Product Owner + Validation Lead | `keep-current` / accepted receipt |
| `RECON-DEC-009` | whether half-open validity becomes the approved public time contract | Scientific Authority + Architecture Lead | open |

2026-08-12 current-branch note：`r0-architecture-agent` 以 `611a48a23ea02ecd0c210a2b101f5c5cbf5df0e6` 为基线实施 R0-SPEC-001 技术冻结，最终技术提交为 `ee7157359e689114d0259a1ae7884a315b029bc1`。`specs/r0-schema-contract-lock.json` 锁定三个 v1 contract；executable evidence locator 收窄为 repository-root-only tracked nonempty regular Git blob；`source_refs` 保持 opaque；PlanProof v1 `premises` 保持 object-map scalar snapshot；产品/runtime consumer 为零。Architecture owner 与独立 Validation reviewer 对同一 13 路径文件集给出接受结论；ADR-0004 与三项 disposition 已接受，R0-SPEC-001 已形成 commit-bound task acceptance。

Prepared slices may test both sides but cannot silently close these decisions。

## 15. Reconciliation mutations

| Mutation | Invalid shortcut | Required rejection |
| --- | --- | --- |
| `RECON-MUT-001` | merge candidate role assignments | virtual/placeholder authority gate |
| `RECON-MUT-002` | copy candidate `done` states | task evidence/owner gate |
| `RECON-MUT-003` | accept ADR by matching filename number | ADR identity/topic/hash gate |
| `RECON-MUT-004` | combine two schema v1 shapes | schema identity/version gate |
| `RECON-MUT-005` | downgrade actual manifest to placeholder to pass | manifest maturity gate |
| `RECON-MUT-006` | mix candidate/current quaternion expected values | convention id/direction gate |
| `RECON-MUT-007` | treat candidate Legacy as independent science truth | reference independence gate |
| `RECON-MUT-008` | duplicate raw logs/CSV without provenance/dedup | artifact control gate |
| `RECON-MUT-009` | transfer candidate CI success to current HEAD | commit/profile identity gate |
| `RECON-MUT-010` | copy generated provenance approvals | rights/authority gate |
| `RECON-MUT-011` | prefer larger term/test count without semantic mapping | criterion evidence gate |
| `RECON-MUT-012` | port test code but omit independent lane/report hash | cross-tool lineage gate |
| `RECON-MUT-013` | promote `packages_user` / `composition_root` into the physical DAG | ADR-0003 and exact registry-shape gate |
| `RECON-MUT-014` | copy candidate responsibility rows as a silent ownership overlay | registry schema and glossary-closure gate |
| `RECON-MUT-015` | generated evidence drops or invents upstream lineage | lineage presence/resolution/DAG gate |
| `RECON-MUT-016` | generated evidence weakens category/classification or external restriction | closed category mapping and blocked-distribution gate |
| `RECON-MUT-017` | copy candidate scientific independence assertion into the aggregate inventory | scientific-context contract and accountable-authority gate |

## 16. Gate impact

Candidate discovery changes the evidence inventory, not the current gate result：

- it proves a public remote branch and a successful candidate hosted run exist；
- it supplies technical comparison material for SPEC/ARCH/SCI/GOV/LEG reviews；
- it exposes an axis-angle representation difference that must enter the scientific difference ledger；
- it does not provide accountable review, current-commit hosted evidence or current-schema executable G1 bundles；
- it does not close any current direct dependency；
- G0/G1 remain not ready；
- R1 remains locked。

## 17. Self-review

- Branch review：verified common ancestor, ahead counts, path counts and 17-path overlap from Git；
- Reproduction review：candidate clean detached worktree bootstrap passed under local MSVC；
- Hosted review：GitHub run URL/result/job counts captured；no claim beyond candidate commit；
- Governance review：virtual labels retained only as evidence of invalid approval, not copied into current roles；
- Contract review：schema incompatibilities listed before any port；no v1 bytes changed；
- Science review：axis-angle difference separated from shared Hamilton/passive algebra；all existing direction expected values remain byte-for-byte unchanged；edge checks exercise Proposed policy without claiming owner acceptance；
- Legacy review：current structured/double-run evidence compared against candidate scope；no raw artifact copied；
- Boundary review：the initial audit changed only reconciliation evidence；subsequent isolated amendments changed governance/spec/architecture/provenance validators and the R0-SCI fixture/oracles, while product paths、public schemas、Legacy、roles、backlog and release state remain unchanged。

## 18. Technical reconciliation closeout

| Stage | Commit | Reviewed outcome |
| --- | --- | --- |
| comparison audit | `59db59b` | isolated candidate reproduction、hosted evidence scope、authority/schema/science/Legacy/provenance differences and selective strategy fixed |
| Slice 0 / GOV-001 | `c16aed9` | distinct `codex-r0-*` virtual seats rejected；real role slots unchanged |
| Slice A / SPEC-001 | `9ce5a85` | strict JSON、actual identity registry and current-contract negative coverage；public schemas unchanged |
| Slice B / SCI-001 | `4c916e1` | quaternion/unit/time edge properties strengthened；current direction fixture and public semantic decisions unchanged |
| Slice C / ARCH-001 | `a9a84c6` | all 27/33 candidate architecture responsibilities classified；exact registry boundaries and 15 mutations；nine-module authority unchanged |
| Slice D / GOV-002 | `7d43a64` | closed current provenance categories、generated lineage/restriction and 14 intended-diagnostic mutations；inventory/right facts unchanged |

At `7d43a64` the worktree was clean。The final implementation-bearing state passed Windows PowerShell 5.1 targeted validators、MSVC x64 Debug 9/9 CTest、Release 9/9 CTest、repository verification（56 JSON、65 task entries、98 Markdown）and staged diff checks。No candidate commit was merged or cherry-picked。

Historical closeout at `7d43a64` did not close authority：at that point all `RECON-DEC-001`～`RECON-DEC-009` remained open，ADR-0004～ADR-0009 remained `Proposed`，and current-commit hosted evidence and owner reviews were absent。

Current amendment on 2026-08-12：ADR-0004/R0-SPEC-001 and ADR-0005/R0-ARCH-001 are accepted through the machine-agent authorization chain and commit-bound review described above。`RECON-DEC-001`～`003`、`006` and `007` are accepted；`RECON-DEC-004`、`005` and `009` remain open。`RECON-DEC-005` now has a technical candidate for `gnczmkn.scientific-context/1`，with the first instance bound to the executable scientific-conventions fixture；its formal disposition remains open until commit-bound owner and independent reviews complete。Rights and external distribution remain fail-closed；R0 gate and R1 remain locked。
