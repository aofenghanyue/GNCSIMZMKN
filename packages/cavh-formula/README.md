# CAVH formula product kernel

本 package 将 `REF-CAVH-FORMULA-001` 已接受的抛物线包络、Eq17、Eq18 与 TDCT 公式迁入独立 R1 产品实现。产品 model identity 为 `gnc.package.cavh.formula.legacy-transcribed.experimental@1`，与 R0 reference identity 保持分离。

当前调用链：

```text
typed immutable formula definition
+ supplied atmosphere / Mach derivative operating point
+ SampleContext
→ analytic parabolic envelope
→ immutable Eq17 or Eq18 gamma reference
→ typed gamma output consumed by TDCT
→ limited formula-stage alpha output with saturation evidence
```

`PreparedCavhFormulaModel` 固定 equation identity、包络参数、适用域阈值和 TDCT 参数。`CavhFormulaKernel` 校验 frame、clock、sample time、configuration revision 与 data quality，返回 Foundation `NumericalOutcome`。Eq17 的垂直升力导数退化会返回 `IllConditioned`，detail 固定为 `eq17-derivative-degenerate-fallback-forbidden`，且不会设置 `FallbackUsed`。

输出中的 `alpha_limited_radians` 仍属于已接受 TDCT 公式的阶段量。R0 科学证据尚未给出它到产品级 guidance command 的 frame、时间和 ownership 映射，因此本 package 不发布最终 guidance contract，也不接入 Session、Compiler、Workflow 或控制面。

直接验证复用现有 R0 oracle：

```powershell
ctest --preset dev -R '^r1\.cavh-formula\.(probe|oracle)$'
```
