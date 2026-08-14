# R0-ARCH-002 架构 Fitness 覆盖与故障设计

- 文档状态：Preparation design
- 实现成熟度：未实现；不得作为 architecture fitness pass evidence
- 任务：`R0-ARCH-002`（backlog 保持 `planned`）
- 日期：2026-08-10
- 权威 owner role：Architecture Lead

## 1. 目标与证据边界

本计划把目标蓝图中的 architecture fitness functions 分成三类证据：当前 R0 可由仓库结构证明的规则、需要 R0 治理 artifact 后启用的规则、需要 R1–R5 产品 artifact 后启用的规则。每条规则必须通过真实 source/descriptor/transaction artifact 或与正向 evaluator 相同的 mutation 证明检测能力。

当前产品是空骨架。零 RuntimeComponent、零 ExecutionPlanDescriptor 或零 StepTransaction 只能证明 artifact 尚不存在，不能证明对应语义正确。coverage report 应使用 `not-applicable-awaiting-artifact` 或 `deferred-by-gate` 表达该状态。

本计划不定义 runtime schema、不增加生产依赖、不改变 module DAG，也不允许扫描器根据类名推断 state owner、temporal relation 或 commit semantics。

## 2. 当前可验证覆盖

| Surface | 当前证据 | 覆盖强度 | 已知缺口 |
| --- | --- | --- | --- |
| 术语与 CapabilityStatus | glossary parser、authority registry、派生 baseline | 强：唯一性、状态、alias、enum authority、hash | 未检查普通 production identifier 与退出词 |
| module DAG | ADR-0003 + CMake target parser | 强：9 modules、22 physical edges、closure、cycle | 未解析 C/C++ include graph；`packages/user/apps` 无独立规则 |
| shared symbol owner | authority registry + authority text | 强：27 个 enum/key/owner identity | 尚无 production definition，可执行 duplicate-definition scan 缺失 |
| Legacy ownership | glossary + 22 条 ownership mapping | 强：唯一 primary owner、consumer、disposition | ownership evidence 不等于 production deletion guard |
| Legacy production dependency | repository verifier 的 3 类 regex | 弱/partial | 删除 token 不完整，路径规范化、注释/字符串和 allowlist 语义未闭合 |
| Kernel/Compiler boundary | CMake closure + 单条 repository regex | 中等 | 其他 module pair 和 source include 方向未闭合 |
| semantic/evolution guards | 无目标 artifact | 未启用 | 需要 maturity、prerequisite 与 mutation 证据 |
| orchestration | CTest + repository verification + fixed-runner workflow | 强：入口闭合 | 新 architecture-fitness validator 尚未接入 |

architecture baseline 当前覆盖 15 项直接 mutation，包括 glossary、term/alias/authority、ownership、DAG/CMake、source hash 和派生 bytes。R0-ARCH-002 只补充可执行切片实际暴露的依赖与边界回归，避免复制第二份 terminology/DAG authority。

### 2.1 Candidate responsibility reconciliation

`origin/codex/r0-first-wave@7d7c0a8` 的 27 个名称 / 33 项职责是 comparison evidence，不是新的 authority：

- 22 项 target module 已落在当前 ownership registry 的 primary owner 或 secondary consumer 集合；
- `execution-phase-plan`、`onboard-state-input-view`、`simulation-builder-plan` 共 3 项提出新的物理 owner 细分；
- `guidance-process-recipe` 与 `node-factory-contribution` 共 2 项使用 `packages_user` 逻辑贡献边界；
- `IContinuousGroup`、`IIntegrator`、`ISummaryObserver`、`math_types.hpp`、`SimulationSummary` 共 5 个当前未登记名称承载 6 项职责；
- 33 项职责引用的 target terms 全部已经存在于当前 glossary，因此差异集中在名称注册、owner 粒度和逻辑边界表示，而不是缺少 canonical vocabulary。

Accepted `RECON-DEC-006` / `RECON-DEC-007` 已固定当前九模块 DAG、22 条 Legacy ownership 和 registry v1 schema。准备表可以使用 `packages/`、`user/`、`apps/`、composition root 等 source-rule label；物理 module 提升、responsibility overlay 或新 Legacy mapping 仍需满足已接受的 superseding/migration 规则。

## 3. 拟采用的守卫层次

```text
authoritative ADR / registry / gate vocabulary
               |
               v
deterministic policy projection
               |
      +--------+---------+
      |                  |
      v                  v
CMake/source inventory   artifact inventory by maturity
      |                  |
      +--------+---------+
               v
findings + coverage states + exact exceptions
               |
               v
positive repository scan + same-evaluator mutations
```

### 3.1 L0｜权威与 maturity

- 继续由 ADR-0003、ADR-0005、glossary 和 authority registry 提供唯一语义来源；
- policy 是确定性投影，不复制术语定义或 module graph；
- policy enforcement state 限定为 `enforced`、`not-applicable-awaiting-artifact`、`deferred-by-gate`，evaluation result 独立记录 `passed`、`failed`、`not-run`；
- rule 从 deferred/awaiting 升为 enforced 时必须同时增加正向目标、负向 mutation、owner task 和 evidence ref；
- public schema、module graph 或 firewall 变化先走 ADR。

### 3.2 L1｜物理依赖与禁入规则

- CMake edge 继续使用当前 parser；
- C/C++ include inventory 只解析预处理 include directive，并将 `gnc/<module>/...` 与可解析相对路径映射到 source owner；
- identifier/deferred-token scan 只扫描 production C/C++/CMake，排除 docs、fixtures、oracles、reports、tests 的 expected violation text 和冻结 Legacy；
- include path 在比较前统一 separator、`.`/`..`、repository root 与 Windows case；越出 repository 的 unresolved relative include 记录为诊断；
- comments/strings 中的历史词不自动形成 identifier finding，include string 在 comment stripping 前单独解析；
- exception 只能针对精确 rule/path/token-or-edge，并带 owner、reason、expiry gate。

### 3.3 L2｜artifact-aware semantic rules

- StateOwner/DecisionAuthority、TemporalRelation、RuntimeInstanceId、obligation、commit/effect、proof/lineage 等规则读取其权威 descriptor 或可执行 fixture；
- artifact 不存在时不使用 source token 猜测语义；
- descriptor 出现后，rule activation 是交付的一部分，不能无限保持 awaiting-artifact；
- 每个 semantic evaluator 同时接受正向 fixture 与最小反例 mutation。

## 4. Fitness family 覆盖计划

| Authority IDs | R0-ARCH-002 计划状态 | 检测依据 | 启用前置 |
| --- | --- | --- | --- |
| FF-ARCH-01、05、06 | 首切片 `enforced` | source owner/include graph、精确 forbidden dependency/token | 依赖任务 done；首切片实现 |
| FF-ARCH-02 | 权威规则 `not-applicable-awaiting-artifact`；R0 只启用 RuntimeCellProfile/已知延期与领域 token 子守卫 | Kernel dispatch/control-flow inventory + reviewed domain vocabulary | 首个真实 Kernel dispatch 进入工作树 |
| FF-ARCH-11 | `not-applicable-awaiting-artifact` | Kernel/Workflow/Control/Artifact dispatch inventory + domain/product vocabulary | 对应 dispatch 首次进入工作树 |
| FF-ARCH-03、04、15 | `not-applicable-awaiting-artifact` | recipe/obligation/RuntimeComponent descriptors 与 publish fixture | R1-MOD/REC/BEH |
| FF-ARCH-07–10、12、16 | R0 follow-on；默认 fail closed | CapabilitySlice/ChangeCard、AuthorityDomain、ChangeVector、proof/operator/commit/evidence refs | governance contract/ADR 与真实 change fixture |
| FF-ARCH-13 | `deferred-by-gate` | Workflow Graph、proof、TaskOutcome、ArtifactCommit | R5-WFP/EXE/ART |
| FF-ARCH-14 | R0 gate evidence | withheld scenario record + reviewed diff/untouched areas | R0-GATE-001 |
| FF-DEP-01–09 | 首切片 `enforced` | source root/include categories + CMake DAG + negative inventory | 依赖任务 done；首切片实现 |
| FF-OBJ-01–07、09、15 | `not-applicable-awaiting-artifact` | ModelDefinition/RuntimeComponent/StateSchema/recipe conformance fixtures | R1-MOD/REC/BEH |
| FF-OBJ-08、12–14 | `not-applicable-awaiting-artifact` | StateBlockPlan、codec、CycleFrame、replacement/commit fixture | R2-BIND/PLAN、R3-STR/TXN |
| FF-OBJ-10、11 | `deferred-by-gate` | Session lifecycle/resource failure suite | R3-LIF |
| FF-BEH-01–10 | `not-applicable-awaiting-artifact` | mechanism/DecisionAuthority/entity/fault descriptors 与 fixtures | R1-BEH/REC、R2-BIND |
| FF-PLAN-01–13 | `not-applicable-awaiting-artifact` | Source/IR/Binding/Plan/Proof compiler positive/negative suite | R2-SRC through R2-PRF |
| FF-RUN-01–10 | `deferred-by-gate` | executable Session transaction/lifecycle/evidence suite | R3-SCH/TXN/CON/LIF |
| FF-DIA-01–04 | `not-applicable-awaiting-artifact` | stable diagnostic registry + failure injection | R1 contracts、R2/R3 diagnostics |
| FF-ART-01–03 | `deferred-by-gate` | RunManifest、lineage、dataset round-trip | R4-OBS/SNK/ART/MAN |
| FF-CFG-01–03 | 分阶段启用 | source/config schema negative suite；runtime image scan | R2-SRC/IR/PLAN 与 R3 |
| §9.8 deletion guard | 首切片 `enforced`，R3/G6 收紧 | exact Legacy identifier/include/path inventory | 当前 AGENTS production 禁入；例外需精确 ADR；G6 零引用 |

### 4.1 Authority ID inventory

下列显式 inventory 只用于证明本计划逐 ID 覆盖治理分册 §9，不复制各 rule 的语义定义：

- Architecture：`FF-ARCH-01`、`FF-ARCH-02`、`FF-ARCH-03`、`FF-ARCH-04`、`FF-ARCH-05`、`FF-ARCH-06`、`FF-ARCH-07`、`FF-ARCH-08`、`FF-ARCH-09`、`FF-ARCH-10`、`FF-ARCH-11`、`FF-ARCH-12`、`FF-ARCH-13`、`FF-ARCH-14`、`FF-ARCH-15`、`FF-ARCH-16`；
- Dependency：`FF-DEP-01`、`FF-DEP-02`、`FF-DEP-03`、`FF-DEP-04`、`FF-DEP-05`、`FF-DEP-06`、`FF-DEP-07`、`FF-DEP-08`、`FF-DEP-09`；
- Object/state：`FF-OBJ-01`、`FF-OBJ-02`、`FF-OBJ-03`、`FF-OBJ-04`、`FF-OBJ-05`、`FF-OBJ-06`、`FF-OBJ-07`、`FF-OBJ-08`、`FF-OBJ-09`、`FF-OBJ-10`、`FF-OBJ-11`、`FF-OBJ-12`、`FF-OBJ-13`、`FF-OBJ-14`、`FF-OBJ-15`；
- Behavior：`FF-BEH-01`、`FF-BEH-02`、`FF-BEH-03`、`FF-BEH-04`、`FF-BEH-05`、`FF-BEH-06`、`FF-BEH-07`、`FF-BEH-08`、`FF-BEH-09`、`FF-BEH-10`；
- Plan：`FF-PLAN-01`、`FF-PLAN-02`、`FF-PLAN-03`、`FF-PLAN-04`、`FF-PLAN-05`、`FF-PLAN-06`、`FF-PLAN-07`、`FF-PLAN-08`、`FF-PLAN-09`、`FF-PLAN-10`、`FF-PLAN-11`、`FF-PLAN-12`、`FF-PLAN-13`；
- Runtime：`FF-RUN-01`、`FF-RUN-02`、`FF-RUN-03`、`FF-RUN-04`、`FF-RUN-05`、`FF-RUN-06`、`FF-RUN-07`、`FF-RUN-08`、`FF-RUN-09`、`FF-RUN-10`；
- Diagnostic：`FF-DIA-01`、`FF-DIA-02`、`FF-DIA-03`、`FF-DIA-04`；
- Artifact：`FF-ART-01`、`FF-ART-02`、`FF-ART-03`；
- Configuration：`FF-CFG-01`、`FF-CFG-02`、`FF-CFG-03`；
- Deletion：`§9.8 deletion guard`。

## 5. Source-root 物理规则

| Source owner/root | 允许的 framework 依赖 | 额外限制 |
| --- | --- | --- |
| `foundation` | `foundation` 内部 | 禁止 runtime/config/logger/filesystem 与 Legacy |
| `contracts` | `foundation`、`contracts` | 禁止 JSON/CSV/CLI 与具体序列化依赖 |
| `model_sdk` | `foundation`、`contracts`、`model_sdk` | 禁止 compiler/kernel/evidence/workflow/application/adapters |
| `compiler` | `foundation`、`contracts`、`model_sdk`、`compiler` | core 禁止具体 JSON/YAML/INI parser 分支；frontend adapter 需独立 root |
| `kernel` | `foundation`、`contracts`、`kernel` | 禁止 compiler/package/domain/format/workflow/frontend |
| `evidence` | `foundation`、`contracts`、`evidence` | sink adapter 不能读取 Runtime Cell/CommittedStateStore |
| `workflow` | `foundation`、`contracts`、`evidence`、`workflow` | 禁止 Session/CycleFrame/state store internals |
| `application` | ADR-0003 closure | 跨域只使用公开 contract/ref/receipt/Outcome |
| `adapters` | foundation/contracts/Application public seam 与已批准 Artifact/Control DTO | 不得依赖 compiler/kernel/workflow internals；composition exception 必须精确 |
| `packages/` | foundation/contracts/model SDK public seam | 禁止 compiler/kernel/workflow/application/adapters；包私有 header 不跨包 |
| `user/` | project composition policy（待真实 consumer 冻结） | framework 永不反向 include；不得迁入通用 runtime |
| `apps/` | composition root | 当前 CLI 只通过 adapters；新增入口复用 Application/Adapter |

表中的“允许依赖”只表达方向上限，不自动授权尚未定义的公共 header。具体 public/private seam 需要在真实 consumer 出现时收窄。

## 6. Legacy 与延期能力策略

### 6.1 R0 production error

- include/link/runtime path 指向 `reference/legacy/`；
- production 使用 `SimulationNode`、`DiscreteNode`、`NodeFactory`、`NodeRegistry`、`AssemblyContext`、`IObservable`、`IDiscreteTask`；
- production 使用 `GNC_REGISTER_NODE_TYPE`、`GNC_REGISTER_BUILTIN_NODE`、`requireByName`、`bindIfPresent`；
- 新 Session/Compiler/Application 调用 Legacy archive 或 extracted binary。

### 6.2 Gate 前 fail closed

- `SegmentTransaction`、`TopologyTransaction`、dynamic package runtime/ABI 的 production definition；
- 用 callback、全局 registry 或 feature flag 模拟上述延期语义；
- 新 `KernelCapability` 没有 F 类场景、ADR、Compiler representation、failure evidence 与 revisit gate。

文档、术语迁移表、测试反例和历史 evidence 可以出现这些名称。例外范围必须精确，不能把整个 `tests/` 或 `docs/` 作为 production allowlist 输入。

## 7. 同一 evaluator 的故障矩阵

未来 validator 应先把 repository 转为标准化 inventory，再让正向检查与 mutation 都调用同一 evaluator。mutation 只改变 inventory 中的 source file、include edge、CMake edge、policy exception 或 maturity record。

最小矩阵为工作包中的 ARCH-MUT-001～016，并增加以下元测试：

- 删除一条 mutation 对应 rule 时，coverage completeness 失败；
- 将零目标的 semantic rule 改为 `passed` 时，maturity consistency 失败；
- exception 缺 owner/reason/expiry 或覆盖多个 path/token 时，exception policy 失败；
- report source hash 漂移时，derived evidence 失败；
- rule id 重复、未知 authority id、未知 activation task/gate 时，policy validation 失败。

## 8. 报告最小内容

- policy/authority input path、normalized SHA-256 与 generator hash；
- module/root/source/include/CMake edge 计数；
- 每个 rule id 的 enforcement state、evaluation result、target count、finding count、mutation count、owner task、activation gate 与 evidence refs；
- 所有 exception 的精确范围与 expiry；
- Legacy/deferred token finding；
- mutation rejection 结果；
- 当前未覆盖项与其 prerequisite artifact；
- runtime consumer count 固定为 0，直到另行 ADR。

## 9. 准备切片退出检查

- 本计划覆盖治理分册 §9 的全部 rule family，没有把未来规则标为已实现；
- 当前强/partial/未启用证据与实际脚本一致；
- 首切片边界可在不修改产品源码和 Legacy 的前提下实施；
- 每个首切片规则有负向注入设计，避免空骨架的 vacuous pass；
- public schema、module/firewall 变化保留 ADR gate；
- Markdown link、UTF-8、repository verification 与 `git diff --check` 通过；
- backlog 状态仍为 `planned`，两个依赖均真实显示为 `done`；任务保持未指派，尚未激活。
