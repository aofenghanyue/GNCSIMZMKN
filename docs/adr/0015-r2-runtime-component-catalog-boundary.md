# ADR-0015: R2 stateless sampled RuntimeComponent Catalog boundary

- Status: Proposed
- Date: 2026-08-19
- Owner: Repository owner
- Related tasks: R2-CAT-001、R2-PLAN-001
- Architecture references: 05 §5～§7、05 §12～§15、10 §4、12 §11～§14、13、14

## Context

R1 已交付 `AltitudePitchGuidanceKernel`。它从 committed rigid observation 独立计算限幅 pitch command，正式 `AltitudePitchGuidanceOutput` 被 `PitchMomentControllerKernel` 消费；两段 YYZ 产品组合在每个 committed boundary 重新求值该链。kernel 没有跨调用 mutable state、reset、checkpoint、termination 或 decision authority。

当前产品 wrapper `ControlledPropelledRigidMassStepKernel` 在调用内部构造 observation，并内联执行 guidance、controller、actuator、propulsion 与 rigid/mass candidate。Catalog 目前没有独立 RigidBody StateOwner/`PublishProjection` provider，也没有 package-owned environment PureQuery；`RunBinding` 只容纳初态、time origin、初始 ParameterState 与外部 replay/input ArtifactRef，不能替代每步 sampled provider。把单个 guidance occurrence 降为计划会留下 required input；把 wrapper 宣称为单一 state owner 又会绕过 rigid/mass 原子边界和未来 R3 transaction ownership。

## Proposed decision

1. `ModelExecutionForm` 增加封闭的 `RuntimeComponent` tag。只有该 form 可以携带 `StaticRuntimeComponentDescriptor`；PureQuery/Closure 继续要求 preparation identity，并拒绝 runtime-only facts。
2. 首个 package-owned descriptor 选择 YYZ `AltitudePitchGuidance`，profile 为 stateless `SampledTransform`，唯一 obligation 为 `BoundaryEvaluation`。placement 固定为 `vehicle.process`。
3. required input 为 current-cycle committed rigid observation，正式 output 为 current-cycle guidance output。两端使用 `SampledSignal`；输入 cardinality 为 exactly-one，输出 cardinality 为 one-or-more。
4. schedule 把现有 R1 两区间产品的“每个 committed boundary 求值一次”冻结为 process phase、整数 step interval 1、offset 0、zero-order hold 和 input age 0。该事实只属于 `gnc.package.yyz.guidance.altitude-pitch.experimental@1`，不声称覆盖 00A/Reference A 的 10/20 Hz 目标 profile；后者需要独立 definition/source 并重新编译。该无状态 descriptor 不开放 state schema；lifecycle 只声明 instantiate/dispose；entry 锁定现有 guidance kernel identity。
5. package 提供 Catalog-only exact config schema 与 builder，确定性保留 frame、clock、configuration revision、guidance gains/limit 和 quaternion policy。该 block 不进入 `CanonicalMissionIr` configuration v1，从而保留 ADR-0013 的现有 IR 范围。科学公式、控制符号、frame、时间边界与 R1 oracle 不变。
6. Catalog 只接受上述已消费组合，并拒绝 invalid form/profile/recipe/obligation/schedule/port/lifecycle。Compiler 解析到 RuntimeComponent definition 后返回 `GNC-PLAN-RUNTIME-COMPONENT-UNAVAILABLE`，直到真实 provider/consumer graph 闭合。
7. RuntimeComponent facts 尚未进入 `CanonicalMissionIr` 或 semantic bytes。`semantic-bytes@2` 显式拒绝 RuntimeComponent execution form，encoding identity 与既有 qualification vector 保持不变。

## Consequences

- `R2-CAT-001` 获得可执行 Catalog snapshot、canonical config round-trip 和非法组合负例，可以进入 owner review。
- `R2-PLAN-001`、`R2-PRF-001`、`R2-LINK-001` 保持 planned。当前不会发布部分 RuntimeComponent IR、未闭合 BindingPlan、虚构 provider、state owner、proof 或 link image。
- sampled output contract 原样锁定现有被 controller 消费的 `AltitudePitchGuidanceOutput`；本 ADR 不把其中的 measured/error/saturation 字段重新分类为未来 `GuidanceCommand` 或 telemetry，稳定 contract 拆分等待真实闭合图。
- 公共 descriptor 只增加首个 consumer 所需的 runtime recipe/profile、obligation、sampled port、schedule、lifecycle 与 algorithm-entry primitive；state-schema primitive 等待真实 state owner。
- Runtime scheduler、Runtime Cell 实例、Session、CommittedStateStore、CycleFrame、StepTransaction、IntegrationScope 和 serializer 保持未实现。

## Alternatives considered

- 把 `RigidStepKernel` 标为 `ContinuousStateOwner`：现有 kernel 自行执行完整 RK4，且没有独立 publish projection、derivative entry、StateSchema、lifecycle 或 environment/mass/wrench provider descriptor，会提前决定 R3 ownership 与 transaction 语义。
- 为 guidance 创建 dummy observation provider 或 package-local runner：无法对应现有产品边界，并会绕过 Plan Firewall。
- 把完整 controlled rigid/mass wrapper 声明为一个 RuntimeComponent：会隐藏 guidance/controller/actuator/propulsion 的真实连接以及 rigid/mass 多 owner 原子提交边界。

## Executable evidence

- `framework/include/gnc/model_sdk/static_descriptor.hpp`
- `framework/include/gnc/compiler/static_mission_compiler.hpp`
- `packages/yyz-rigid-step/src/mass_commit.cpp`
- `tests/compiler_runtime_component_catalog.cpp`
- `r2.compiler-runtime-component-catalog.probe`
