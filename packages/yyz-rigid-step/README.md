# YYZ rigid step

这是首个可执行的 R1 YYZ 产品切片，产品 identity 为 `gnc.package.yyz.rigid-step.frozen-interval.experimental@1`。

当前调用链：

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

当前范围仅覆盖 `FrozenInterval` 内保持的环境、质量属性、气动表和 supplied wrench。推进、燃耗提交、控制面/速率导数、跨区间 commit 与运行期调度仍在范围外。

直接验证入口：

```powershell
ctest --preset dev -R '^r1\.yyz-rigid-step\.(probe|oracle)$'
```

oracle 检查直接读取 `REF-YYZ-FROZEN-INTERVAL-001` 及其 80 位 Decimal reference，对气动查询、力矩闭合、刚体导数和 tick 1 candidate 使用原 fixture 声明的容差。
