# ADR-0016: R2 portable query and closure execution-input groundwork

- Status: Proposed
- Date: 2026-08-19
- Owner: Repository owner
- Related tasks: R2-BIND-001、R2-PLAN-001、R2-PRF-001、R2-LINK-001
- Architecture references: 04 §15.1、05 Pass 9、05 §10.1～§10.2、12 §5.2、12 §9.1～§9.2、14 §24

## Context

当前 R2 静态编译已经从真实 CAVH/YYZ package descriptors 形成 canonical Mission IR、BindingPlan、TemporalBindingPlan、binding proof 和 query/closure obligations。旧的窄 `ExecutionPlanDescriptor` 只保存 prepare identity 和 obligation endpoint；query/closure kernel identity、workspace fact 与完整 preparation inputs 没有进入 portable lowering，因此未来 plan/link pass 仍需重新查询 Catalog 或猜测执行合同。

完整 RuntimeComponent plan 仍被一个更早的产品合同缺口阻断：当前 YYZ wrapper 在函数内部临时构造 committed rigid observation，产品尚无 package-owned RigidBody StateOwner/StateSchema、initial mapping、`PublishProjection` 和 DerivativeEvaluation entry。直接把 wrapper 或 RigidStep 标为 state owner 会提前决定 R3 state/time/transaction ownership。本决定只完成与该缺口无关的 portable descriptor groundwork。

## Proposed decision

1. Contracts 成为 `ClosureStrategy` 的唯一语义 owner，并声明 `FrozenInterval | CandidateState | AlgebraicSolve` 的完整共享枚举。枚举存在不等于当前 Compiler 支持对应执行能力。
2. Model SDK 为 PreparedModel-backed execution forms 增加封闭、互斥的 `StaticPureQueryDescriptor` 与 `StaticClosureDescriptor`。两者保存 exact stable entry id/version 和显式 workspace requirement；Closure 另保存 `ClosureStrategy`。这些字段不保存函数地址、ABI 或 PreparedModel 实例。
3. 当前三个真实 kernel 均无 caller-visible workspace，因此 CAVH GlideEnvelope query、YYZ AerodynamicTable query 和 ForceMomentClosure 都精确声明 `workspace=None`。不为未来可能的 workspace 预留虚构 size/alignment/layout。
4. Catalog 通用验证 execution form 与 query/closure/runtime payload 恰好匹配，并要求 exact prepare/entry identity。当前 Closure slice 只接受产品已验证的 `FrozenInterval + IntervalModel`；CandidateState 与 AlgebraicSolve fail closed。验证不按 CAVH/YYZ model id 写特殊分支。
5. `ExecutionPlanDescriptor` revision 3 保存：
   - 每个 occurrence 的 `PreparedModelPreparationInputs`：stable input id、package/model/form、exact prepare identity、完整 canonical config、已解析 asset-binding refs 和稳定去重的 source refs；
   - PureQuery 的 `QueryExecutionSpecInputs`：preparation-input ref、exact query entry、显式 workspace，以及从已解析 BindingPlan 派生的 consumer-binding facts；
   - Closure 的 `ClosureExecutionSpecInputs`：同级 stable facts，加 exact strategy 与 consumer temporal relation；
   - 每项 consumer-binding fact 的完整 provider/consumer endpoint、contract、scope resolution 和 binding source；
   - 每个 compiled obligation 到对应 query/closure execution input 的稳定引用。
6. consumer-binding facts 只说明 query/closure response 的结果流。普通 AlgorithmKernel consumer 不因此获得 `BoundQuerySet`；closure invocation caller 也不从 provider→consumer edge 推导。正式 caller authorization 等待 `QueryPlan`、`ClosurePlan`/`IntegrationScopePlan` 与 link pass。
7. Source refs 聚合 occurrence、subject/scope/placement、configuration root、逐字段配置、asset binding、Catalog execution descriptor 与 consumer binding provenance，并按 URI/path 去重排序。source relocation 改变 provenance，但不改变 semantic hash 或 source-independent dry-run explain。
8. Canonical Mission IR revision 与 `semantic-bytes@2` 保持不变。Entry identity 是 package implementation/plan fact，不进入当前 canonical source semantics。

## Consequences

- Descriptor 已能在不重新查询 Catalog 的情况下说明当前 PureQuery/Closure 的 stable entry、preparation input、workspace fact、strategy 和 response consumer binding。
- `PreparedModelPreparationInputs` 不是完整 `PreparedModelPlan`：当前没有 `PreparedModelKey` 所需的 config/asset/numerical/preparation policy hashes，也没有 cache/lifetime policy。
- `QueryExecutionSpecInputs`/`ClosureExecutionSpecInputs` 是正式 `QueryPlan`/`ClosurePlan` 的编译输入。当前没有 authorized invocation caller、numeric slot、ABI/layout hash、linked function entry、Bound handle、IntegrationScope、workspace allocation、RuntimeInstance 或 Session。
- 既有 `BindingProof` 不升级为 `PlanProofRecord`/`PlanProofIndex`，本决定也不宣称 link 成功。`R2-PLAN-001`、`R2-PRF-001` 与 `R2-LINK-001` 继续保持 planned。
- RuntimeComponent firewall 不变；任何 RuntimeComponent source 仍以 `GNC-PLAN-RUNTIME-COMPONENT-UNAVAILABLE` fail closed，且不发布局部 IR/plan。

## Alternatives considered

- 只在 obligation 保存非空 kernel 字符串：无法冻结 workspace、strategy、preparation reference 或 response consumer binding，并迫使后续 pass 重查 Catalog。
- 直接生成完整 `PreparedModelPlan`：缺少 accepted `PreparedModelKey`、policy hashes 与 cache/lifetime 输入，会把不完整 key 固化为公共合同。
- 在 Descriptor 保存函数地址或 workspace instance：混淆 portable Descriptor、进程内 Image 和 Session bindings 三层 ownership。
- 同时开放 CandidateState/AlgebraicSolve：现有产品只证明 FrozenInterval；提前接受会声明不存在的 closure executor、solver 与 transaction capability。

## Executable evidence

- `framework/include/gnc/contracts/execution_semantics.hpp`
- `framework/include/gnc/model_sdk/static_descriptor.hpp`
- `framework/include/gnc/compiler/static_mission_compiler.hpp`
- `packages/cavh-formula/include/cavh/formula.hpp`
- `packages/yyz-rigid-step/include/yyz/rigid_step.hpp`
- `tests/compiler_static_plan.cpp`
- `r2.compiler-static-plan.probe`
