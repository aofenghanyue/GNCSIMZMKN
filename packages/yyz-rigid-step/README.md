# YYZ rigid step and mass boundary

本 package 目前包含两个连续的 R1 YYZ 产品切片：单段刚体 candidate，以及两段刚体/标量燃耗的 typed atomic-boundary composition。两条产品 model identity 分别为 `gnc.package.yyz.rigid-step.frozen-interval.experimental@1` 和 `gnc.package.yyz.two-interval-mass-commit.experimental@1`。

单段刚体调用链：

```text
typed committed rigid state
+ typed environment and mass properties
+ prepared Mach/alpha/beta aerodynamic table
+ supplied body wrench
→ strict trilinear query
→ aerodynamic dimensionalization and CoM closure
→ full-inertia rigid derivative
→ fixed RK4 interval candidate
```

`RigidStepModelDefinition` 在准备边界校验固定步长、数值策略、frame/clock identity、气动几何和表格；`PreparedRigidStepModel` 持有不可变定义与表格存储。`RigidStepKernel` 只接收显式 typed 输入并返回 candidate，不读取 Session、Mission、文件系统、logger 或全局 registry。

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
```

首条 oracle 检查直接读取 `REF-YYZ-FROZEN-INTERVAL-001`；第二条读取 `REF-YYZ-TWO-INTERVAL-MASS-COMMIT-001`。两者都使用原 fixture 声明的容差，reference 与产品 model identity 保持分离。
