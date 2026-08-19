# YYZ rigid step and mass boundary

本 package 目前包含两个连续的 R1 YYZ 产品切片：单段刚体 candidate，以及两段刚体/标量燃耗的 typed atomic-boundary composition。两条产品 model identity 分别为 `gnc.package.yyz.rigid-step.frozen-interval.experimental@1` 和 `gnc.package.yyz.two-interval-mass-commit.experimental@1`。

单段刚体调用链：

```text
typed committed rigid state
+ typed environment and mass properties
+ prepared AerodynamicTable PureQuery and real table asset
+ supplied body wrench
→ formal strict trilinear query output
→ aerodynamic dimensionalization
→ pure ForceMomentClosure output and RigidFormInput
→ full-inertia rigid derivative consumes the held form input
→ fixed RK4 interval candidate
```

`PreparedAerodynamicTableModel` 是 YYZ 当前真实 PureQuery：独立 definition 保存气动几何与 table asset identity，独立 asset 保存 Mach/alpha/beta axes 和 coefficient rows，prepare 形成 immutable trilinear view。`AerodynamicTableQueryKernel` 的正式 output 只提供六个 coefficients，domain status 与 weights 留在 telemetry；`RigidStepKernel` 直接消费该 output 并保持既有 dimensionalization 数值。

`PreparedForceMomentClosureModel` 是当前唯一使用 `Closure` execution form 的 YYZ 模型。`ForceMomentClosureKernel` 规范化 contribution 顺序，拒绝重复 source 与 frame/clock/revision/interval 不一致，逐项完成 CoM 力矩搬移，并以正式 output 提供 total force/moment 和 `RigidFormInput`。`RigidStepModelDefinition` 在准备边界校验固定步长、数值策略、frame/clock identity、气动几何和真实表格 asset；`PreparedRigidStepModel` 持有不可变定义、AerodynamicTable query model 与 Closure model。`RigidStepKernel` 只接收显式 typed 输入并返回 candidate，不读取 Session、Mission、文件系统、logger 或全局 registry。

package descriptor 将 AerodynamicTable 声明为 `vehicle.output + PureQuery`，ForceMomentClosure 声明为 `interaction/closure + ContinuousClosureLink + IntervalModel`，并提供各自 canonical config schema。RigidStep 的两个 required input 分别要求 exact PureQuery 与 closure contract。对应 builder 从稳定 block 重建 typed definition；aero asset slot 以 exactly-one `AssetBinding` 接受 `gnc.asset.yyz.aerodynamic-table.multiaffine@1`。这些字段由通用 Compiler 读取，package id 不进入 Compiler 分支。

同一 package contribution 现在也把真实 `AltitudePitchGuidanceKernel` 描述为首个 RuntimeComponent Catalog 候选。该 kernel 对每份 committed rigid observation 做独立求值，正式 output 被 `PitchMomentControllerKernel` 消费；descriptor 因而冻结 stateless `SampledTransform + BoundaryEvaluation`、process phase 每步 schedule、current-cycle sampled ports、zero-order hold、空 state schema、instantiate/dispose 和 exact kernel identity。canonical config builder 保留 frame、clock、revision、三项 guidance 参数与完整 quaternion policy。

当前 `ControlledPropelledRigidMassStepKernel` 仍在 package wrapper 内构造 observation 并顺序调用 guidance/controller/actuator，package 尚未贡献可独立连接的 committed-observation provider、environment/run-binding provider 或完整 runtime component 图。Catalog descriptor 可以独立审查；Compiler 会拒绝把孤立 guidance occurrence 降为 RuntimeComponent plan。

两段边界调用链：

```text
opening committed rigid state + MassState
+ held environment and supplied body wrench
+ typed MassFlowInterval
→ RigidStepKernel reads opening committed mass
→ ScalarBurnMassKernel produces a constant-geometry mass candidate
→ both candidates form one AtomicRigidMassCandidate
→ complete candidate becomes the next committed boundary value
→ interval 1 consumes that rigid state and mass
```

`FrozenRigidMassStepKernel` 只在刚体与质量两项 candidate 全部成功后返回值。`TwoIntervalMassCommitKernel` 把完整 candidate 提升为下一段 opening boundary；调用者无法向第二段单独注入陈旧质量或陈旧刚体状态。产品实现保持独立 identity，并由 `ORACLE-YYZ-TWO-INTERVAL-MASS-COMMIT-001` 的 80 位 Decimal 分段解析轨迹直接比较。

当前 atomic boundary 是 package 内的纯值组合，不拥有 mutable state store、epoch 或 Session rollback。运行期 `ModelCommit`、失败后保留上一成功边界、推进查询、控制面/速率导数、fuel geometry、depletion event、Compiler 与调度继续留在后续阶段。

直接验证入口：

```powershell
ctest --preset dev -R '^r1\.yyz-rigid-step\.(probe|oracle)$'
ctest --preset dev -R '^r1\.yyz-two-interval-mass-commit\.(probe|oracle)$'
ctest --preset dev -R '^r2\.compiler-runtime-component-catalog\.probe$'
```

首条 oracle 检查直接读取 `REF-YYZ-FROZEN-INTERVAL-001`；第二条读取 `REF-YYZ-TWO-INTERVAL-MASS-COMMIT-001`。两者都使用原 fixture 声明的容差，reference 与产品 model identity 保持分离。
