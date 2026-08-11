# 参考附录｜全库术语注册表

[返回主入口](README.md) · [进入系统主叙事](02-layered-reference-architecture.md) · [专家评审处置](专家评审处置.md)

**主线定位**：本表登记跨册稳定词汇、唯一拼写、生命周期关系、交付状态和权威分册。概念推导以 02 为权威；字段级定义以表中“权威”列所指分册为权威。

## 1. 注册与引用规则

1. 代码/Schema 名使用表中的反引号拼写；中文正文可以使用对应中文释义，不能创造第二个英文类型名。
2. 新跨册术语先在本表登记唯一名称、定义、状态和 owner，再进入其他分册。
3. 同一概念改名时一次性更新目标文档；旧名只留在“退出名/兼容关系”表和迁移证据中。
4. 两个对象处于不同生命周期阶段时分别保留，并明确 `source -> compiled plan -> runtime instance -> evidence` 转换。
5. `V1` 表示第一代目标承诺；`Stable` 表示跨代稳定语义；`PressureOnly` 只验证兼容边界；`Deferred` 需要独立 capability gate；`Legacy` 只描述当前代码或迁移输入。
6. 共享枚举、cache key、identity 与 commit 结构只在一份权威分册中完整定义，其他分册链接引用。

## 2. 已仲裁的退出名与近义关系

| 退出名或易混写法 | 唯一名称/关系 | 决定 |
| --- | --- | --- |
| Component Recipe | `RuntimeCellRecipe` | 全库统一改名 |
| PortBindingPlan | `BindingPlan` | 全库统一改名 |
| ContinuousClosureGroup | `IntegrationScopePlan` | 目标对象退出；连续成员和 closure 进入 scope plan |
| IContinuousGroup | `IContinuousGroup`（Legacy）→ `IntegrationScopePlan` | 当前源码迁移源，不能出现在目标 API |
| Solver Island | `SolverIslandPlan` | 保留为 scope 内可选联立求解子计划，不与 IntegrationScopePlan 合并 |
| InstantStateEffect | `InstantPatch` | 全库统一改名 |
| IntervalStateEffect | `IntervalCandidate` | 全库统一改名 |
| StepDiagnostic | `DiagnosticDraft` | 数值/模型边界先生成 draft |
| RenderedDiagnosticSnapshot | `RenderedDiagnostic` | 全库统一改名 |
| diagnostic bundle | `DiagnosticBundleArtifact` | 语义对象；物理 artifact type 为 `diagnostic-bundle` |
| ObservationBatch draft | `ObservationDraft` | seal 前对象 |
| ObservationProjectionPlan vs ObservationPlan | 父子关系 | 前者是后者编译后用于 projector 的精确子计划 |
| MissionPatch | `MissionSourcePatch` | authoring proposal；批准并物化后可产生 `CompilePatch` |
| RequiredOperational | `OperationalTelemetry` 或 `RequiredMetricInput` | 按数据用途拆分 |
| Invalidated / Incomplete | `EvidenceValidity` + reason | 只作状态变化原因，不能作枚举成员 |
| closure proof | `PlanProofRecord` / `PlanProofIndex` | 证明语义与物理 Closure 词族分离 |
| authority（裸词） | `AuthorityDomain` / `StateOwner` / `DecisionAuthority` / `PermissionGrant` | 按语义限定 |
| profile（裸词） | `RuntimeCellProfile` / `RunProfile` / `TrainingProfile` / `FidelityLevel` | 按使用场景限定 |
| capability（权限语义） | `PermissionGrant` | `KernelCapability`、`BackendCapability` 保留技术语义 |
| Execution Plan Descriptor | `ExecutionPlanDescriptor` | 前者只作中文正文的空格排版，不形成第二类型 |

## 3. 架构分析与治理

| 唯一名称 | 状态 | 定义 | 权威 |
| --- | --- | --- | --- |
| `ResearchWorkbench`（研究工作台） | Stable | 覆盖问题定义、模型供给、编译、执行、分析、论证和报告的整体产品 | 00 |
| `AuthorityDomain` | Stable | `Design/Plan \| Model \| Operation \| Artifact` 四类权威提交边界 | 02 |
| `StateOwner` | Stable | 对一份可变模型事实拥有唯一写入与提交责任的对象 | 02、12、14 |
| `DecisionAuthority` | V1 | 对导航源、飞行阶段、构型等共享选择拥有唯一决策责任的窄 owner | 13 |
| `PermissionGrant` | V1 | 允许 actor 对明确资源执行明确操作的授权凭证 | 10 |
| `CapabilitySlice` | Stable | 复杂需求按权威事实拆出的最小架构分析单元 | 02 |
| `ChangeVector` | Stable | `<V,G,S,T,I,R,X>` 七维语义变化描述 | 02 |
| `ChangeCard` | V1 | A–E 类普通变化使用的简化评审记录，包含 owner、接缝、非零维度、稳定区和证据 | README、02 |
| `KernelCapability` | Stable | Model Authority 现有算子无法表达的新通用时间、原子性、rollback 或 effect 语义 | 02 |
| `BackendCapability` | V1 | 执行后端声明的线程、设备、deadline、rollback、transport 等能力 | 02、10 |
| `CapabilityStatus` | V1 | `Stable \| V1 \| PressureOnly \| Deferred \| Legacy` 的交付状态 | README、11 |
| `PlanFirewall` | Stable | 作者输入与 executor 之间只能通过已编译计划交接 | 00、02、05 |
| `CommitFirewall` | Stable | 每个 owner 只能通过本域事务提交权威事实 | 00、02、06、14 |
| `ArtifactControlFirewall` | Stable | 执行路径只通过 typed command、receipt、Outcome、Observation 与 ArtifactRef 连接 | 00、02、08–10 |

## 4. 作者输入、模型供给与编译

| 唯一名称 | 状态 | 定义 | 权威 |
| --- | --- | --- | --- |
| `MissionSource` | V1 | 人或工具编写的场景、模型、参数、运行和观测配置文档集合 | 05 |
| `SourceFrontend` | V1 | 把 JSON/YAML/受限 INI/蓝图/API 解析为语法中性的 SourceTree/SourceMap | 05 |
| `SourceFrontendPort` | V1 | SourceFrontend 向 Compiler 交付 SourceTree、SourceMap、SourceBlob refs 与 ParseOutcome 的格式中性端口 | 05 |
| `SourceTree` | V1 | 与输入编码解耦的作者结构树 | 05 |
| `SourceMap` | V1 | SourceTree 节点到原文 URI、路径和 span 的映射 | 05 |
| `MissionSourcePatch` | V1 | 针对 versioned MissionSource 的 typed authoring proposal | 05、10 |
| `CompilePatch` | V1 | 经批准并由 Experiment/case materializer 物化、在编译前作用于允许路径的 typed patch | 05、09 |
| `DefinitionRef` | Stable | 对带 id/version 的 Model、Algorithm、Task 或其他 Definition 的稳定引用 | 05、08 |
| `EntityId` | Stable | Model Graph 与运行证据中一项物理或逻辑实体的稳定身份 | 04、05、14 |
| `ModelOccurrenceId` | Stable | Mission/IR 中一次 ModelDefinition 选择的稳定身份；不等同于运行实例 | 05、12 |
| `SessionId` | Stable | 一次 Simulation Session 实例的稳定身份 | 05、06 |
| `RunId` | Stable | Session 中一次 initialize/reset/restore attempt 及其成功 run 的全局唯一身份 | 06、08 |
| `RuntimeInstanceId` | Stable | ExecutionPlanDescriptor 为 RuntimeComponent occurrence 分配的 plan-local slot id；与 SessionId 组成 RuntimeCell identity | 04、05、12 |
| `ParameterId` | Stable | 跨 Experiment、RunBinding、command、manifest 与 lineage 保持稳定的参数身份 | 05、09 |
| `ParameterSet` | V1 | 物理、资产和算法参数的可复用作者输入 | 05 |
| `RunProfile` | V1 | dt、duration、数值、确定性和资源策略的作者输入 | 05 |
| `ObservationPlan` | V1 | 观测字段、采样、criticality、sink 和指标意图的完整计划 | 05、08 |
| `CanonicalModelGraph` | Stable | 实体、ModelOccurrence、owner、关系、绑定意图与研究意图的规范图 | 02、05 |
| `MissionIR` | V1 | CanonicalModelGraph 的 typed、版本化、可持久化中间表示 | 05 |
| `ModelPackage` | V1 | 带版本、依赖、资产、Definition、测试和成熟度声明的发布单元 | 05、10 |
| `PackageManifest` | V1 | package identity、依赖、贡献、实现和验证元数据 | 05 |
| `ModelDefinition` | V1 | Catalog 中可选择模型的稳定配置、端口、执行形式和证据声明 | 05、12 |
| `AlgorithmDefinition` | V1 | AlgorithmKernel 的不可变参数、公式选择、限制和数值策略引用 | 12 |
| `CompiledModelOccurrence` | V1 | Mission 中一次模型选择经过归一化、资产绑定和实现锁定后的不可变记录 | 05、12 |
| `StateSchema` | V1 | owner state blocks 的字段、类型、shape、单位、初始化、codec 与 invariant 契约 | 05、12、14 |
| `EntitySelectorDescriptor` | V1 | 跨实体读取的 selector、cardinality、visibility、inactive policy、frame 与 temporal contract | 04、05 |
| `VariationTargetDescriptor` | V1 | 拉偏、扰动或故障目标的 ParameterId、单位、合法域、物化时机、owner 与恢复规则 | 05、09 |
| `OutputSchema` | V1 | RuntimeComponent 可发布 output 的稳定字段、类型、单位、time 与 quality 契约 | 08、12 |
| `PortDescriptor` | V1 | typed endpoint 的 contract、direction、cardinality、scope、time、unit/frame 与 quality 声明 | 04、05 |
| `PureQuery` | V1 | 对 immutable PreparedModel 的显式只读查询语义；无 Session state、schedule 或副作用 | 04、12 |
| `PureQueryDescriptor` | V1 | ModelDefinition 中声明 PureQuery execution form 的 tagged descriptor | 05、12 |
| `RuntimeComponentDescriptor` | V1 | 独立运行边界的 state、port、obligation、lifecycle 和 resource 声明 | 05、12 |
| `RuntimeCellRecipe` | V1 | 把 AlgorithmKernel、StateFragment、mechanism、port 与 obligation 合成为一个运行边界的 package 定义 | 12、13 |
| `RuntimeCellProfile` | V1 | SDK 提供的 recipe 组合模板；Compiler 最终展开为 obligations | 12 |
| `RuntimeComponent` | V1 | execution form 声明的独立运行边界 | 12 |
| `RuntimeCell` | V1 | RuntimeComponent 在某个 Session 中的实例，identity 为 `(SessionId, RuntimeInstanceId)` | 12 |
| `BindingPlan` | V1 | 已解析的 typed producer-consumer edge、adapter、cardinality、time 与 handle 计划 | 04、05 |
| `ExecutionPlanDescriptor` | V1 | portable、immutable、可序列化的完整执行语义与 proof refs | 05 |
| `ExecutionPlanImage` | V1 | Descriptor 与精确实现 link 后形成的进程内只读 entry/handle 布局 | 05 |
| `PlanRef` | V1 | 对 immutable plan descriptor/image identity 的稳定引用 | 02、05、10 |
| `PlanProofRecord` | V1 | Compiler 对一项 identity/owner/causality/time/state/resource/evidence 断言的结构化结果 | 05 |
| `PlanProofIndex` | V1 | graph element、plan element、source span 与 PlanProofRecord 之间的可查询索引 | 05 |
| `PreparedModel` | V1 | prepare 阶段生成、immutable、可由多个 Session 安全共享的模型数据产品 | 05、06、12 |
| `PreparedModelKey` | V1 | PreparedModel cache identity；字段固定为 definition id/version、canonical occurrence config hash、asset-set hash、implementation id/version、numerical-policy hash、preparation-policy hash | 05 |
| `SessionRuntimeBindings` | V1 | Session 私有的 RuntimeCell、state、workspace、resource handle 与 prepared refs | 05、06 |
| `RunBindingSchema` | V1 | Descriptor 允许的单次运行输入字段、类型、单位、约束和 identity 规则 | 05 |
| `RunBinding` | V1 | 经 RunBindingSchema 验证的初态、seed、time origin、ParameterState 与输入引用 | 05、06 |

## 5. 模型、数值与 step 执行

| 唯一名称 | 状态 | 定义 | 权威 |
| --- | --- | --- | --- |
| `AlgorithmKernel` | V1 | 只接收显式 definition/state/input/workspace 并返回 result/delta 的纯模型计算 | 12 |
| `ClosureKernel` | V1 | 在显式 sample/candidate context 上求物理闭合、无可变实例状态与副作用的纯 kernel | 12、14 |
| `RuntimeState` | V1 | 某 Session 中会影响未来模型结果、必须支持 reset/checkpoint/replay 的 owner 状态 | 12 |
| `StateFragment` | V1 | 嵌入宿主 RuntimeCell、并入其 StateSchema 的局部行为状态片段 | 13 |
| `FaultStateFragment` | V1 | 宿主 RuntimeCell 内记录 fault mode、触发、恢复和局部退化历史的 StateFragment | 13 |
| `EmbeddedMechanism` | V1 | 无独立 Session identity、port、schedule 和 lifecycle 的局部行为工具 | 13 |
| `ModeState` | V1 | 宿主 owner 内记录当前离散 mode、进入 tick 与局部 transition history 的 StateFragment | 13 |
| `ComponentDelta` | V1 | 一个 obligation invocation 提议的 owner state patch、output、event、telemetry 和 diagnostic drafts | 12、14 |
| `AlgorithmResult` | V1 | AlgorithmKernel 返回的 typed output、next local state、telemetry 与 diagnostic facts | 12 |
| `StateReplacement` | V1 | 带 state block handle、base epoch、invocation id 和 commit class 的完整 owner-block replacement payload | 12、14 |
| `InstantPatch` | V1 | commit class 为 InstantAtTk、在当前 committed boundary 原子替换 owner state 的 StateReplacement | 14 |
| `IntervalCandidate` | V1 | commit class 为 IntervalAtTk1、描述 `[t_k,t_{k+1}]` 演化后候选 owner state 的 StateReplacement | 14 |
| `ContinuousCandidate` | V1 | integrator/solver 对连续 state block 产生的候选状态 | 14 |
| `CommittedModelState` | Stable | 某个 state_epoch/tick 上已原子提交的物理、离散、控制与实体生命周期事实 | 02、14 |
| `CommittedStateStore` | V1 | 保存 owner committed state blocks 的 Session store | 06、14 |
| `CommittedOutputStore` | V1 | 保存跨 tick held sampled outputs 的 Session store | 06、14 |
| `CommittedCommandLedger` | V1 | 保存 command admission/maintenance 的 committed sequence 与状态、不属于模型 state block 的控制存储 | 06、14 |
| `SessionCommandQueue` | V1 | 保存已受理且尚未终结的 command entries，并在 safe point 提供冻结 cutoff view 的控制存储 | 06、14 |
| `EventQueue` | V1 | 按 EventDeliveryPlan 保存、排序和消费事件的可 checkpoint 控制存储 | 06、14 |
| `SourceRuntimeState` | V1 | ExternalEndpoint 中会影响下一批 payload/quality 的 cursor、dedup watermark 与重放状态 | 06、12、14 |
| `CycleFrame` | V1 | 单个 StepTransaction 内由编译 handle 访问的 typed signal/interval slot frame | 14 |
| `InputFrameView` | V1 | Session 为一个 obligation callsite 授权的 CycleFrame slot-handle 只读视图，随 StepTransaction 失效 | 06、08、14 |
| `InputBundleView` | V1 | Runtime Cell entry 从 InputFrameView 投影出的领域 typed 输入视图，直接传给 AlgorithmKernel | 12、14 |
| `OutputWriterSet` | V1 | 一个 obligation callsite 获得的 Descriptor-authorized CycleFrame writer tokens 集合 | 14 |
| `QueryHandleSpec` | V1 | Descriptor 中 query occurrence、contract 与 workspace layout 的 portable handle 规格 | 04、12 |
| `LinkedQueryEntry` | V1 | ExecutionPlanImage 中已解析到精确 query kernel implementation 的只读 entry | 04、12 |
| `BoundQueryHandle` | V1 | ExecutionPlanImage 中绑定 QueryPlan、PreparedModel 与纯 kernel 的运行期只读 handle | 04、12、14 |
| `BoundQuerySet` | V1 | 一个 plan callsite 获得的窄 query handle 集合，只允许调用 Descriptor 授权的 queries | 04、12 |
| `ClosureHandleSpec` | V1 | Descriptor 中 closure occurrence、contract 与 workspace layout 的 portable handle 规格 | 12、14 |
| `LinkedClosureEntry` | V1 | ExecutionPlanImage 中已解析到精确 ClosureKernel implementation 的只读 entry | 12、14 |
| `BoundClosureHandle` | V1 | SessionRuntimeBindings 将 ClosureHandleSpec、LinkedClosureEntry 与 PreparedModel 组合后的只读 handle | 12、14 |
| `StateCodecEntry` | V1 | ExecutionPlanImage 中 state block 的 size/alignment/clone/swap/validate/encode/decode/project 函数表 | 12、14 |
| `SlotCodecEntry` | V1 | ExecutionPlanImage 中 CycleFrame slot 的 size/alignment/copy/validate/project 函数表 | 14 |
| `StepJournal` | V1 | 本 step 的 staged patch、event、receipt、diagnostic 与 commit 决策记录 | 14 |
| `StepTransaction` | Stable | 从一个 committed boundary 到下一个 boundary 的整步原子事务 | 06、14 |
| `ModelCommit` | Stable | 将已验证 owner state/output、tick 与相关 staged receipt 原子发布为新 state_epoch 的提交 | 02、06、14 |
| `InitializationCommit` | V1 | 首次 initialize 成功后原子建立 run、初态、初始 output 与 state_epoch=0 的提交 | 06 |
| `ResetCommit` | V1 | reset 成功后原子建立新 RunId、替换初态并重置 tick 的提交 | 06 |
| `RestoreCommit` | V1 | checkpoint 候选 store 全部验证后原子建立分支 run 的提交 | 06 |
| `CommandLedgerCommit` | V1 | 提交 command admission、expiry 或 supersession，只推进 ledger sequence、不改变 state_epoch | 06、14 |
| `CommandSubmissionOutcome` | V1 | command 入口的 `Enqueued \| SubmissionRejected` 与关联 Diagnostic；不证明模型已应用 | 04、06 |
| `CommandLedgerMaintenanceReceipt` | V1 | safe point 上 Expired/Superseded 等 command queue 维护回执 | 04、06 |
| `CommandApplicationReceipt` | V1 | owner 在 due tick 产生并随 ModelCommit 提交的 Applied/Rejected/Deferred 回执 | 04、06、14 |
| `state_epoch` | Stable | 每次成功 ModelCommit 单调递增的模型状态 revision | 06、08、14 |
| `tick` | Stable | 当前 run 内 committed grid boundary 的逻辑步序号 | 06、14 |
| `ExecutionObligation` | Stable | Kernel 可执行的 `PublishProjection \| BoundaryEvaluation \| IntervalEvolution \| DerivativeEvaluation \| SourceFreeze \| PostCommitEffect \| ResourceLease` 基础义务 | 02、04、12 |
| `PublishProjection` | V1 | 从 committed owner state 生成 t_k truth/state projection 的 ExecutionObligation | 02、04、12 |
| `BoundaryEvaluation` | V1 | 在 boundary DAG 上以 committed state 与 typed inputs 产生 ComponentDelta 的 ExecutionObligation | 02、04、12 |
| `IntervalEvolution` | V1 | 为 `[t_k,t_{k+1}]` 产生离散区间演化候选的 ExecutionObligation | 02、04、12 |
| `DerivativeEvaluation` | V1 | 在 IntegrationScopePlan candidate stage 求导数或 closure outcome 的 ExecutionObligation | 02、04、12 |
| `SourceFreeze` | V1 | 在 safe point 截止 ExternalEndpoint 输入并产生 immutable batch/cursor candidate 的 ExecutionObligation | 02、04、12、14 |
| `PostCommitEffect` | PressureOnly | 只消费 committed Outcome/effect intent 并生成幂等外部效果 receipt 的 ExecutionObligation | 02、04、10、14 |
| `ResourceLease` | V1 | 在 session/run lifecycle 内显式登记 acquire、rollback 与 close 的 ExecutionObligation | 02、04、06、12 |
| `ExecutionRegionPlan` | V1 | `Publish \| Boundary \| Integration \| Commit \| PostCommit` 区域的有序 obligation callsites、safe point 与 transaction branch 计划 | 05、14 |
| `ObligationCallsitePlan` | V1 | 一个已编译 obligation 调用点的 cell、entry、handle、state access 与稳定调用序号 | 05、14 |
| `BoundaryDagPlan` | V1 | 同一 boundary 上 current-cycle typed edges 的拓扑执行计划 | 05、14 |
| `TemporalBindingPlan` | V1 | edge 的 TemporalRelation、rate、freshness、latency、hold 与 adapter 计划 | 05、14 |
| `PortSlotPlan` | V1 | CycleFrame typed slot 的 writer、readers、storage 与 hold policy | 14 |
| `StateBlockPlan` | V1 | state block 的 schema、StateOwner、初始化与 delta kind 计划 | 05、14 |
| `PreparedModelPlan` | V1 | occurrence 到 prepare factory、PreparedModelKey、cache 与 lifetime 的计划 | 05、06、14 |
| `QueryPlan` | V1 | query contract、prepared handle、pure kernel、workspace 与授权 caller 的编译计划 | 05、14 |
| `CommandRoutePlan` | V1 | command 的 DecisionAuthority、queue、effective point、owner route 与 receipt 计划 | 05、14 |
| `EventDeliveryPlan` | V1 | event 的 delivery point、稳定排序与 consumer set 计划 | 05、14 |
| `EntityTopologyPlan` | V1 | entity/group/relation/selector、activation mapping 与 topology revision 计划 | 05、14 |
| `RegimeMappingPlan` | V1 | 两个 evolution regime 之间的完整 state mapping、event/time、continuity 与 invariant 计划 | 14 |
| `InterventionPlan` | V1 | ParameterId/CommandId 的允许 target、物化类别、domain、recovery 与 evidence 要求 | 05、09、14 |
| `TransactionPlan` | V1 | terminal/continue/failure 分支的 commit sets、invariants、receipts、seal 与 effect hooks | 05、14 |
| `TemporalRelation` | Stable | `CurrentCycle \| PreviousCommitted \| HeldLatest \| IntervalModel \| CandidateStateQuery \| EventAtOrBefore` 数据时序关系 | 04、14 |
| `IntervalModel` | V1 | 对 `[t_k,t_{k+1}]` 有效、通过 TemporalRelation 交付的只读参数或纯 callable contract | 14 |
| `IntervalModelWrite` | V1 | ComponentDelta 或 ClosureOutcome 对 transaction-local IntervalModel slot 的受权写入 | 14 |
| `SampledSignal` | V1 | 带 sample/effective time、quality 与 hold/freshness 规则的 typed port kind | 04、05 |
| `AssetBinding` | V1 | Model occurrence 到 immutable、versioned asset ArtifactRef 的编译绑定种类 | 04、05 |
| `AtGrid` | V1 | 事件或转换只在已提交 grid boundary 生效的首版定位策略 | 06、14 |
| `IntegrationScopePlan` | V1 | 由一个 integrator 协同推进并在同一事务提交的连续 state members、closure、events、workspace 和 policy | 03、06、14 |
| `SolverIslandPlan` | V1 | IntegrationScopePlan 或 boundary region 内需要联立求解 residual/constraints 的封闭子计划 | 02、14 |
| `IContinuousGroup` | Legacy | 当前源码中声明 RK 子步共享候选状态的接口；R3 切换后退出目标 API | 01、11 |
| `ClosureDescriptor` | V1 | package/model 侧纯 closure kernel 的配置、输入、适用策略和验证声明 | 12 |
| `ContinuousClosureLink` | V1 | BindingPlan 中把 candidate state、query kernel 和 held inputs 接入 IntegrationScopePlan 的边类型 | 04、14 |
| `ClosureStrategy` | V1 | `FrozenInterval \| CandidateState \| AlgebraicSolve` 三种连续闭合策略 | 14 |
| `ClosurePlan` | V1 | Compiler 对 ClosureDescriptor、link 和 strategy 的已解析调用计划 | 05、14 |
| `NumericalPolicy` | V1 | 容差、迭代上限、外推、非有限值、确定性等数值选择 | 03 |
| `NumericalStatus` | V1 | `Success \| Converged \| Approximate \| OutOfRange \| Extrapolated \| NoBracket \| MaxIterations \| Singular \| IllConditioned \| DomainError \| NonFiniteInput \| NonFiniteIntermediate \| NonFiniteOutput \| StepUnderflow \| ToleranceUnreachable \| Cancelled \| InternalFailure` 数值结果分类 | 03 |
| `NumericalOutcome` | V1 | 数值算法统一返回的 status、value、residual、iteration、flags 和 diagnostics facts | 03 |
| `RootProblem` | V1 | 求根问题的函数、导数、区间/初值、scale 和 policy 定义 | 03 |
| `QueryResult` | V1 | 纯查询返回的 value、domain status、quality、gradient 和 telemetry | 03、12 |
| `Quality` | Stable | 数据在来源、时效、有效域、故障和估计置信度方面的 typed 状态 | 04 |
| `FidelityLevel` | V1 | 模型或分析的物理/数值保真度选择及其适用域 | 03、10 |
| `ControlAnalysisResult` | V1 | 带 plant/controller/operating-point identity、线化矩阵、特征量、裕度、适用域和诊断的控制分析 Artifact payload | 03 |
| `Evaluator` | V1 | 产生 EvaluationResult、metrics、Event 或 TerminationDecision 的 RuntimeCellProfile | 04、12 |
| `TerminationDecision` | V1 | evaluator 对 Continue/Complete/Abort/Invalidate 的结构化模型判定，带稳定 reason、time、subject 和 metrics | 06 |
| `ParameterState` | V1 | Session 中由唯一 StateOwner 持有、可经受控 command 更新的运行期参数状态 | 05、12 |
| `TunableParameterDescriptor` | V1 | ParameterState 中允许在线修改字段的 id、type、unit、range、effective point 与 policy 声明 | 05、10、12 |
| `ParameterUpdateReducer` | V1 | 校验调参 command，并为 ParameterState 产生事务 patch 的纯 reducer | 10、12 |
| `RunResourceOpenHook` | V1 | initialize/reset/restore attempt 中建立 run-scoped resource lease 并登记 rollback/close action 的 hook | 04、06 |
| `RunFinalizeHook` | V1 | 已提交 run 结束时执行领域 flush/protocol 并返回 Outcome、但不拥有 lease close 的 hook | 04、06 |
| `ModelPrepareFactory` | V1 | 从 CompiledModelOccurrence 与 immutable assets 构造 PreparedModel 的 package factory | 04、06 |
| `RuntimeCellFactory` | V1 | 从 descriptor、PreparedModel 与 compiled handles 创建 Session-local RuntimeCell 的 package factory | 04、06 |
| `InstanceResourcePrepareHook` | V1 | 建立 run-invariant、session-scoped endpoint/resource 的 lifecycle hook | 04、06 |
| `InitialStateBuilder` | V1 | 从 validated RunBinding 无副作用地构造首次 run 候选 state/output 的 builder | 04、06 |
| `ResetStateBuilder` | V1 | 从新 RunBinding 无副作用地构造 reset 候选 replacement state/output 的 builder | 04、06 |
| `DisposeHook` | V1 | 释放 session-scoped resource、不得创建新 run 或改写 RunOutcome 的 lifecycle hook | 04、06 |
| `SampledTransform` | V1 | 读取当前 inputs/state 并产生 sampled output 的 RuntimeCellProfile | 12 |
| `DiscreteStateProcessor` | V1 | 在 boundary 上更新离散 owner state 与 output 的 RuntimeCellProfile | 12 |
| `ContinuousStateOwner` | V1 | 拥有连续 state block，并通过 IntegrationScopePlan 推进的 RuntimeCellProfile | 12 |
| `Coordinator` | V1 | 汇聚多个 candidate/command 并持有窄 DecisionAuthority 的 RuntimeCellProfile | 12、13 |
| `FlightPhaseDecisionCell` | V1 | 持有共享飞行阶段 StateOwner/DecisionAuthority，并发布 revisioned phase snapshot 的参考 RuntimeCell | 12、13、15 |
| `VehicleConfigurationDecisionCell` | V1 | 持有共享构型 StateOwner/DecisionAuthority，并发布 ConfigurationSnapshot 的参考 RuntimeCell | 13、15 |
| `NavigationDecisionCell` | V1 | 在多个导航 candidate 间执行唯一交班 DecisionAuthority，并发布 NavigationEstimate/source snapshot 的参考 RuntimeCell | 13、15 |
| `PreparedAeroModel` | V1 | prepare 阶段生成的 immutable 气动网格、索引、插值系数与适用域产品 | 03、12、15 |
| `FaultCommand` | V1 | 触发、恢复或调整模拟故障的 typed command；只能由目标 owner/reducer 应用 | 04、13 |
| `NavigationEstimate` | V1 | 导航链对状态、协方差/quality、frame、sample time 与 source revision 的唯一正式估计输出 | 04、13、15 |
| `NavigationCandidate` | V1 | 单个导航器件或算法发布的 estimate candidate、health、coverage、covariance 与 freshness contract | 13、15 |
| `NavigationSourceSnapshot` | V1 | NavigationDecisionCell 发布的当前 source、handover state 与 revision 快照 | 13、15 |
| `GuidanceCommand` | V1 | guidance 到 controller 的 tagged、带 basis revision/time/quality 的正式命令 contract | 04、12、15 |
| `ActuatorCommand` | V1 | allocator/controller 到 actuator 的 typed command contract，携带构型 basis revision | 13、15 |
| `ActuatorCommandDisposition` | V1 | actuator 对旧构型命令执行 reject、neutralize 或 typed remap 后发布的结构化结果 | 13、15 |
| `ConfigurationSnapshot` | V1 | VehicleConfigurationDecisionCell 发布的 configuration id、phase/progress 与单调 revision 快照 | 13–15 |
| `ActivateEntityRequest` | V1 | v1 对预编译 inactive EntityId 提交的原子激活 intent；随 ModelCommit 生效 | 13–15 |
| `TemporalContract` | V1 | port/endpoint 对 sample/effective time、rate、freshness、latency、hold 与 clock 的声明 | 04、06 |
| `PublishedClosureSample` | V1 | 在声明 sample point 物化、可供 observation/diagnostics 使用且不受 RK stage 次数影响的闭合样本 | 01、14 |
| `ContinuousLocated` | Deferred | 需要 SegmentTransaction 才可对模型产生步内 jump 的连续事件定位策略 | 06、14 |

## 6. 诊断、Outcome 与证据

| 唯一名称 | 状态 | 定义 | 权威 |
| --- | --- | --- | --- |
| `DiagnosticCodeSpec` | V1 | code、message key、参数/evidence schema、默认 severity/disposition、waiver policy 和文档链接的静态定义 | 07 |
| `DiagnosticDraft` | V1 | 单次 kernel/compiler/tool 调用产生的 call-local 问题事实 | 07 |
| `DiagnosticRecord` | V1 | 经 context enrich、获得稳定 id 后不可变的结构化问题事实；不拥有最终 severity、处置或 validity effect | 07 |
| `DiagnosticBatch` | V1 | Compilation/Step/Task transaction 内稳定排序的 DiagnosticRecord ids 与 primary candidate | 07 |
| `DiagnosticBundleArtifact` | V1 | 持久化 records、batches、policy decisions、code refs 与可选 RenderedDiagnostic 的 Artifact | 07、08 |
| `RenderedDiagnostic` | V1 | 指定 locale/template version 下从 DiagnosticRecord、DiagnosticCodeSpec 与可选 PolicyDecision 派生的展示结果 | 07 |
| `DiagnosticPolicy` | V1 | DiagnosticRecord + operation state 到 PolicyDecision 的纯规则集 | 07 |
| `DiagnosticAggregationPolicy` | V1 | 高频相同问题的 dedup key、bucket、首次/阈值/摘要发射规则 | 07 |
| `PolicyDecision` | V1 | DiagnosticPolicy 派生的 matched rule、最终 severity、disposition、validity effect 和 action payload | 07 |
| `Waiver` | V1 | 对明确 code/scope/version/期限和风险接受人的受控 policy 例外 | 07 |
| `OutcomeEnvelope<T>` | V1 | status、optional value、primary/related diagnostics、validity 与 artifact refs 的通用结果 | 07 |
| `ParseOutcome` | V1 | SourceFrontend 的 `Parsed \| Failed`、SourceTree/SourceMap refs 与 diagnostics | 05、07 |
| `CompilationOutcome` | V1 | 一次 Mission/Workflow compile 的 `Succeeded \| SucceededWithWarnings \| Failed`、PlanRef、proof index、diagnostics 与 source refs | 05、07 |
| `LinkOutcome` | V1 | Descriptor 与 exact implementations link 成 ExecutionPlanImage 的结构化结果 | 05、07 |
| `QueryOutcome` | V1 | PureQuery/Closure 调用的 status、optional QueryResult、quality 与 diagnostics 结果 | 04、07、12 |
| `PreparationOutcome` | V1 | model/resource prepare 的 `Prepared \| Partial \| Failed \| Cancelled` 与 cleanup journal 摘要 | 06、07 |
| `SessionCreateOutcome` | V1 | PlanRef 成功 link 并建立 Created Session 时返回 SessionHandle；失败时携带 diagnostics | 06、07、10 |
| `InitializationOutcome` | V1 | initialize attempt 是否形成 InitializationCommit、RunId、initial observation 与 diagnostics 的结果 | 06、07 |
| `ResetOutcome` | V1 | reset attempt 是否形成 ResetCommit、新 RunId 与 initial observation 的结果 | 06、07 |
| `RestoreOutcome` | V1 | checkpoint restore attempt 是否形成 RestoreCommit 与 branch Session 的结果 | 06、07 |
| `CheckpointOutcome` | V1 | CheckpointBarrier 与 checkpoint Artifact 是否成功提交的结果 | 06、07 |
| `FinalizationOutcome` | V1 | run finalize/lease close 的 `Succeeded \| Failed`、primary/related diagnostics 与 evidence impact | 06、07 |
| `RecordOutcome` | V1 | RecordSink reserve/enqueue/write/flush/close 调用的 durability、drop 与 diagnostics 结果 | 07、08 |
| `ExternalEffectOutcome` | PressureOnly | ExternalEffectCommit 的 applied/failed/unknown 与 idempotency receipt 结果 | 06、07 |
| `EvidenceOutcome` | V1 | Observation/Artifact durability、coverage、validity 与 related ArtifactRefs 的证据域结果 | 06、08 |
| `RunOutcome` | V1 | 一次 run 的 lifecycle status、terminal reason、last commit、validity、diagnostics 和 ArtifactRefs | 06、07 |
| `StepOutcome` | V1 | 一个 StepTransaction 的 commit/rollback/cancel 结果与 diagnostics | 06、07、14 |
| `TaskOutcome` | V1 | 一个 Workflow Task 的状态、attempt、receipts、validity 和 output ArtifactRefs | 09 |
| `WorkflowOutcome` | V1 | Workflow Plan 的最终/部分完成状态及 required outputs | 09 |
| `EvidenceValidity` | Stable | `Valid \| ValidWithCaveats \| Partial \| Invalid \| Unknown`；所有 Observation、Outcome 和 Artifact 共用 | 07 |
| `EvidenceCriticality` | Stable | `CriticalEvidence \| RequiredMetricInput \| OperationalTelemetry \| BestEffortDisplay \| DebugOnly` | 08 |
| `ObservationKind` | V1 | `InitialAtT0 \| CycleAtTk \| RestoredAtCheckpoint`；terminal step 仍使用 CycleAtTk，区分同一采样时刻的 batch 语义 | 08 |
| `ObservationDraft` | V1 | seal 前、transaction-owned 的观测候选 buffer 与 metadata | 08、14 |
| `ObservationBatch` | V1 | 与一个 commit/采样窗口绑定、sealed immutable 的列式观测单元 | 08 |
| `ObservationProjectionPlan` | V1 | ObservationPlan 编译出的 field handles、projectors、schema 和 commit binding 子计划 | 05、08、14 |
| `ObservationSeal` | V1 | 把 ObservationDraft 与 ModelCommit 原子关联并形成 ObservationBatch 的动作 | 06、08、14 |
| `ExternalEffectCommit` | PressureOnly | ModelCommit 后执行受计划约束的外部效果并产生幂等 receipt/outcome 的提交边界 | 06、10 |
| `EvidenceCommit` | V1 | 将 sealed observation/outcome/manifest 的 durability 状态提交到证据域的边界 | 02、06、08 |
| `ArtifactCommit` | V1 | Artifact Store 对 staged payload 执行验证、hash、lineage 写入与原子 publish 的提交 | 02、08、09 |
| `FieldDescriptor` | V1 | FieldId、类型、shape、unit、frame、time、quality 与 stability 元数据 | 08 |
| `FieldId` | Stable | 跨编码、报告和查询保持稳定的观测字段身份 | 08 |
| `EncodingPlan` | V1 | Observation schema 到 CSV/MAT/HDF5/stream payload 的确定映射 | 08 |
| `RecordSink` | V1 | 异步消费 sealed ObservationBatch 或相关 Outcome，并声明 reservation、backpressure、flush 与 failure 语义的端口 | 08 |
| `DatasetSink` | V1 | 使用 EncodingPlan 写入 durable dataset payload，并在 ArtifactCommit 后发布 ArtifactRef 的 RecordSink | 08 |
| `InMemoryObservationSink` | V1 | 提供有界 process-local batch/ring buffer 或只读数组 view、不自动提交 Artifact 的 RecordSink | 08 |
| `LiveObservationSink` | PressureOnly | 发布带 schema/time/sequence 的 best-effort 或 operational 传输帧、不拥有模型状态的 RecordSink | 08、10 |
| `OnlineMetricSink` | V1 | 按 MetricDefinition 累积 MetricResult draft，并在 finalize 后提交 metric Artifact 的 RecordSink | 08 |
| `RecordPipeline` | V1 | 在 ObservationSeal 后路由 immutable batches/outcomes 到 RecordSink，并协调 backpressure 与 EvidenceCommit 的管线 | 08 |
| `MetricDefinition` | V1 | metric 的输入 FieldId/Artifact、窗口、算法版本、单位、适用域与判据定义 | 08、09 |
| `MetricResult` | V1 | MetricDefinition 求值后带 value、unit、coverage、validity、diagnostics 与 lineage 的结果 | 08、09 |
| `Artifact` | Stable | 可寻址、带 schema、hash、validity 与 lineage 的研究产物 | 08 |
| `ArtifactRef` | Stable | 只指向 committed Artifact 的不可变引用 | 08 |
| `PublishRef` | V1 | 对 ModelCommit、ObservationSeal、ArtifactCommit 等已发布事实的 typed reference | 02、08 |
| `ArtifactDescriptor` | V1 | Artifact identity、type、schema、hash、producer、inputs、validity 与 storage metadata | 08 |
| `LineageEdge` | V1 | 从 input ArtifactRef 到 output ArtifactRef 的 typed provenance edge，含 role、producer operation、parameters 和 commit refs | 08 |
| `EvidenceGraph` | Stable | Observation、Outcome、Artifact 和 LineageEdge 组成的研究证据权威图 | 02、08 |
| `RunManifest` | V1 | run/session identity、source/plan/proof/binding/package/build/commit/outcome/artifact 的运行复现清单 | 08 |
| `EvidenceBundle` | V1 | 面向交付的输入、计划、运行、分析、报告、manifest 和 lineage 集合 | 08 |

## 7. Workflow、Application 与前端

| 唯一名称 | 状态 | 定义 | 权威 |
| --- | --- | --- | --- |
| `WorkflowDefinition` | V1 | research purpose、typed inputs、task graph、policy 和 expected outputs 的作者定义 | 09 |
| `WorkflowPlan` | V1 | 经 task/tool 解析、DAG/proof/resource 检查后冻结的 immutable 计划 | 09 |
| `TaskDefinition` | V1 | 一个可复用工作流任务的输入输出、参数、执行、资源、失败和 assurance 声明 | 09 |
| `WorkflowTask` | V1 | TaskDefinition 在某个 Workflow operation 中的计划节点/attempt | 09 |
| `ToolDescriptor` | V1 | 外部工具身份、版本、平台、许可证和能力声明 | 09 |
| `ToolAdapter` | V1 | InputRenderer、InvocationBuilder、ProcessRunner、OutputCollector、OutputParser、ScientificValidator、Normalizer 等窄部件组合 | 09 |
| `ProjectModelExtension` | V1 | 项目私有模型、算法与可选 RuntimeComponent 的扩展面 | 10 |
| `ModelPackageExtension` | V1 | 通过 PackageManifest 与 Catalog 贡献稳定组件、资产和验证的扩展面 | 10 |
| `DomainPackageExtension` | V1 | 按真实项目贡献领域契约、模型包、模板和验证套件的扩展面 | 10 |
| `NumericalExtension` | V1 | 通过 Numerical Descriptor 接入数值算法后端的扩展面 | 03、10 |
| `WorkflowTaskExtension` | V1 | 通过 TaskDefinition 与 Artifact contract 增加分析、工具或报告任务的扩展面 | 09、10 |
| `ToolAdapterExtension` | V1 | 通过 ToolAdapter 窄部件接入 DATCOM、MATLAB、GPOPS2 等外部工具的扩展面 | 09、10 |
| `LanguageAdapterExtension` | V1 | 通过 Application API 或受限 RuntimeCell Adapter 接入 Python/Lua 的扩展面 | 10 |
| `FrontendAdapterExtension` | PressureOnly | 通过 Control API 与 read-model/stream 边界接入 CLI、GUI 或游戏引擎的扩展面 | 10 |
| `ApplicationControlPlane` | V1 | 向 CLI/Python/LLM/Studio/实时前端提供统一 command、query、event 和 operation lifecycle | 10 |
| `OperationId` | V1 | 长操作的稳定 identity | 10 |
| `OperationReceipt` | V1 | compile/run 等长操作的受理结果、OperationId 与后续 Outcome 查询入口 | 10 |
| `SessionHandle` | V1 | Application API 暴露的 opaque Session 引用；不泄露 Runtime Cell 或 store 地址 | 10 |
| `CommandLedger` | V1 | Application command 的 admission、dedup、queue 与 receipt 权威记录 | 10、14 |
| `ResearchProposal` | V1 | actor 提交的 typed 研究/authoring/operation 提案封套 | 10 |
| `ResearchQuestion` | V1 | 研究目标、假设、比较对象、判据与所需证据的结构化定义 | 05、08、09 |
| `TrainingProfile` | V1 | RL 训练时的 randomization、observation、reward 和 resource 配置 | 10 |
| `RenderSnapshot` | PressureOnly | committed model state 的实时只读投影；不拥有模型状态 | 10 |
| `ClockPlan` | PressureOnly | 多 clock domain 的 rate、offset、hold/interpolation 与同步关系计划；待异步纵向案例冻结字段 | 04、15 |
| `TimeScale` | PressureOnly | UTC/TAI/TT/TDB 等时标 identity、epoch 与转换 provenance contract；待卫星项目冻结 | 03、10、15 |
| `Ephemeris` | PressureOnly | 带 body/frame/time-scale identity 与 provenance 的星历查询 contract；待卫星项目冻结 | 04、10、15 |
| `EntityPrototype` | Deferred | TopologyTransaction 可创建实体的计划锁定 prototype、package 与资源边界 | 14、15 |
| `ExternalEndpoint` | V1 | 受计划约束的外部 source/effect 资源边界 | 06、10、12 |
| `Experiment` | V1 | 一组共享研究问题、参数空间和比较规则的运行集合 | 08、09 |
| `ExperimentDefinition` | V1 | Experiment 的 base source/plan、ParameterSpace、采样、target、资源、聚合与完成规则 | 09、10 |
| `CaseId` | Stable | 由 ExperimentDefinition hash、规范化参数值与 replicate key 派生的 case 身份 | 09、10 |
| `CaseMaterialization` | V1 | 一个 case 的规范参数、CompilePatch、RunBinding、command schedule、seed 与 provenance 记录 | 09、10 |
| `CaseParameterTarget` | V1 | ParameterId 到 CompilePatch、RunBindingPatch 或 RuntimeCommandSchedule 的唯一物化分类与约束 | 08–10 |
| `CaseManifest` | V1 | 一个 Experiment case 的参数、patch、binding、seed、plan/run identity 和结果 | 08 |
| `RunBindingPatch` | V1 | 只改变 RunBindingSchema 允许字段的 case 变化 | 09、10 |
| `RuntimeCommandSchedule` | V1 | 不改 plan/binding、按时间提交 typed command 的可复现输入流 | 09、10 |

## 8. 能力状态与未来词汇

| 能力 | 状态 | 当前承诺 | 开启条件 | 权威 |
| --- | --- | --- | --- | --- |
| 固定步长、AtGrid event、静态拓扑 | V1 | 第一条 YYZ 运行主线 | R3 gate | 06、14 |
| 多 Session、Experiment、Python reset/step | V1 | 第一代研究消费者 | R6 gate | 10、11 |
| LLM proposal 与 Blueprint authoring | V1 | 只生成 proposal/source 并经过 Compiler/approval | R7 gate | 10、11 |
| `RenderSnapshot` 与软实时前端 | PressureOnly | 验证 command/snapshot/backend 接缝 | R8 真实 consumer | 10、11 |
| HIL/设备 effect | PressureOnly | 只冻结 ExternalEndpoint、receipt 和 failure 边界 | 延迟/rollback/resource 证据 | 06、10 |
| `SegmentTransaction` | Deferred | 保留 unsupported diagnostic 和 capability gate | located jump 真实纵向案例 | 06、14 |
| `TopologyTransaction` | Deferred | v1 使用预声明 entity activation | 未知实例/动态 solver membership 案例 | 06、14 |
| 动态 package ABI | Deferred | v1 使用静态 PackageSet | 独立部署/第三方生态证据 | 05、10 |
| 多机分布式执行 | Deferred | v1 使用本地 worker/backend | 容量、隔离或许可证约束 | 09、10 |
| 高级天文时标/星历/星座规模 | PressureOnly | 保留 TimeScale/Ephemeris contract 压力要求 | 实际卫星项目 | 03、04、10、15 |

## 9. Legacy 迁移词汇

本表只帮助阅读现状审计、删除台账和行为 oracle。Legacy 名称不能进入目标 API、Mission IR 或 ExecutionPlanDescriptor。

| Legacy 名称 | 状态 | 目标去向 | 权威 |
| --- | --- | --- | --- |
| `SimulationNode` | Legacy | `ModelDefinition` + execution form + `RuntimeCellRecipe` | 01、迁移治理 |
| `NodeFactory` | Legacy | Package Catalog 中的 definition/factory contribution | 01、05 |
| `NodeRegistry` | Legacy | `ExecutionPlanImage` 的 typed handles 与 Session-local stores | 01、迁移治理 |
| `FrameworkCatalog` | Legacy | versioned Package/Catalog snapshot | 01、05 |
| `AssemblyContext` | Legacy | Compiler lowering contexts + `BindingPlan` / plan handles | 01、迁移治理 |
| `ConfigNode` | Legacy | `SourceTree`、typed definitions 与 compiled occurrence config | 01、05 |
| `ConfigReader` | Legacy | SourceFrontend schema validation + package definition builder | 01、迁移治理 |
| `ConfigManager` | Legacy | SourceFrontend + Mission Compiler | 01、迁移治理 |
| `MissionAssembler` | Legacy | Mission Compiler passes + plan linker | 01、05 |
| `SimulationBuilder` | Legacy | Application compile/link/create-session use case | 01、05 |
| `ExecutionPhaseManager` | Legacy | `ExecutionRegionPlan` + `BoundaryDagPlan` executor | 01、14 |
| `IDiscreteTask` | Legacy | `BoundaryEvaluation` obligation | 01、迁移治理 |
| `DiscreteNode` | Legacy | `RuntimeComponentDescriptor` + sampled obligations | 01、迁移治理 |
| `IObservable` | Legacy | `ObservationProjectionPlan` + typed projectors | 01、08 |
| `ObservableField` | Legacy | `FieldDescriptor` / `FieldId` | 01、08 |
| `IRecordSink` | Legacy | `RecordSink` | 01、08 |
| `AutoDataLogger` | Legacy | `RecordPipeline` + `DatasetSink` | 01、08 |
| `SimFlow` | Legacy | `ExperimentDefinition` + case materialization + workflow executor | 01、09 |
| `OnboardState` | Legacy | typed Measurement/Estimate/Command contracts | 01、15 |
| `OnboardStateProcess` | Legacy | Navigation/Guidance/Control Runtime Cells 与 typed edges | 01、15 |
| `GuidanceProcess` | Legacy | guidance six-piece + `RuntimeCellRecipe` | 01、12、15 |
| `Simulator` | Legacy | Simulation Session collaborators + scientific comparison runner；纵向切换后退出 | 01、06、迁移治理 |
