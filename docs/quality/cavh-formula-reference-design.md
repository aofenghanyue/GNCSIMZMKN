# REF-CAVH-FORMULA-001：CAVH 公式与制导 reference bundle 设计

## 1. 文档目的与权威边界

本文定义 [`R0-SCI-004`](../tasks/work-packages/R0-SCI-004.md) 在依赖和 owner decision 关闭后应如何构造、验证与审查 [`REF-CAVH-FORMULA-001`](../../fixtures/ref-cavh-formula/fixture-manifest.json)。本文不在准备阶段宣布某篇论文、某组方程或冻结实现正确；目标是让以下失败在 closed-loop execution 之前被稳定隔离：

1. 来源、edition、页码、license 或 equation map 错误；
2. Eq17/Eq18 转录、符号、单位或 intermediate 错误；
3. atmosphere/Mach/最优升阻比导数定义或数值条件错误；
4. glide envelope 的域、边界、唯一性、插值或优化误差错误；
5. denominator、非有限、非物理输入或 undeclared fallback 错误；
6. TDCT error 符号、gain、单位、限幅和 quality 错误；
7. target、Legacy 和 independent reference 发生同源或反向依赖；
8. 闭环总体指标掩盖上游局部错误。

本文中的公式全部标为下列三种状态之一：

- `source_verified`：已由 Scientific Authority 对指定 edition/page/equation 审批；
- `normalized_candidate`：从候选来源独立转录，等待第二转录者与 owner 签署；
- `legacy_transcription_only`：只描述冻结代码当前计算什么，不声称与论文一致。

截至 2026-08-10，本文列出的 Eq17/Eq18 代数仅为 `legacy_transcription_only`。本设计不会把外部网页元数据、Legacy 注释、测试函数名、运行结果或后续书籍章节提升为公式权威。

## 2. Governing blueprint 与来源清单

### 2.1 仓库内规范来源

- [15 参考纵向设计](../../design-notes/gnczmkn-architecture-roadmap/15-reference-vertical-designs-and-object-placement.md) §10 要求把 CAVH 拆成 definition/binding、prepared glide envelope、environment derivative query、Eq17/Eq18 kernel、TDCT、command、telemetry 与逐层验证；
- [09 科学复现与工程适配](../../design-notes/gnczmkn-architecture-roadmap/09-research-workflows-and-tool-adapters.md) §16 要求 citation、claim、normalized equation、symbol、assumption、scenario、reference data、algorithm、discrepancy、verification、engineering wrapper 与 maturity 可追溯，并把 faithful reproduction、independent verification、engineering adaptation 分成不同 artifact；
- [`R0-SCI-001`](../tasks/work-packages/R0-SCI-001.md) 准备了 SI、角度、binary64、整数 tick、domain 与数值失败的候选约定，但在 owner 接受前仍不是最终 science policy；
- [`R0-LEG-001`](../tasks/work-packages/R0-LEG-001.md) 和[复现证据索引](../../reference/legacy/reproduction/r0-leg-001-20260810-07/evidence-index.json)只提供冻结行为证据；
- [`REF-CAVH-FORMULA-001` manifest](../../fixtures/ref-cavh-formula/fixture-manifest.json)要求 citation/version、assumption、Eq17、Eq18、derivative、glide envelope、TDCT 和 closed-loop artifacts。

### 2.2 外部出版物元数据候选

| Source id | Metadata | 当前可用结论 | 当前不可声称 |
| --- | --- | --- | --- |
| `CAVH-SRC-AIAA-2011` | Wenbin Yu, Wanchun Chen, *Guidance Scheme for Glide Range Maximization of a Hypersonic Vehicle*, AIAA 2011-6714, DOI [`10.2514/6.2011-6714`](https://doi.org/10.2514/6.2011-6714)；[BUAA 作者成果页](https://teacher.buaa.edu.cn/yuwenbin/zh_CN/lwcg/157999/content/19603.htm) | 题名、作者、会议、年份和 DOI 是高可信候选元数据 | 本仓库 Eq17/Eq18/Eq19 的字节、页码、符号和数值已对应此论文 |
| `CAVH-SRC-SPRINGER-2020` | Wanchun Chen 等，*Trajectory Damping Control Technique for Hypersonic Glide Reentry*，Springer 章节，DOI [`10.1007/978-981-15-8901-0_9`](https://doi.org/10.1007/978-981-15-8901-0_9) | 存在后续系统性章节，可用于版本史和交叉审查 | 章节公式编号与 2011 论文编号相同，或可替代主来源 edition |

激活后，source record 至少包含：完整 citation、edition/version、publisher landing page、合法获取方式、license/use restriction、raw bytes 或受控 external ref、raw SHA-256、页码范围、每个 equation/figure/table 的 locator、获取人与时间、审批 ref。若不能合法把原文纳入仓库，应保存受控外部位置和可复核 hash，不得抓取或规避访问限制。

### 2.3 本次审计固定的本地 bytes

| Artifact | Raw SHA-256 | 解释 |
| --- | --- | --- |
| Fixture manifest | `d89f4e9517aeb0e6a7eebecf3191b37aef29a1a875a5e125f9db72007e891ac6` | 只固定 seed manifest |
| Blueprint 15 | `7e01ec0c882c97293af15442440caa91d68ade8f5e51972a36c39c981acb576d` | 目标拆分与验证边界 |
| Blueprint 09 | `559fa8d8c882ca5a882070655c993fa4c64aa856eedc0c946c68588853459a95` | 科学复现证据纪律 |
| Frozen guidance | `b58bbe5df7e2f428e74a998e76ea5a6b3d3fcfd459f5e97bc99137f3aead51eb` | Legacy 行为源，不是 paper truth |
| Frozen guidance test | `6f9856c7073b00046e49cbce707010f0dfc809199ac292156dec929332c0d745` | 暴露同源 expected 风险 |
| Frozen mission | `c06128ab6fd5a060c2904df7fdbd2bebb59ba44dd3d7c54976fbbeef1c80a305` | Legacy scenario/config bytes |
| Frozen aero CSV | `56284e7e9221cada63e24a3066f631d54041dd6510603dca4a4b561ae4da9c52` | mission 实际使用的数据 |
| Frozen paper-aero source | `19a5730e4e507171e24f12f578ce3b2769089aaff81768f4366c2df747b34349` | 另一份硬编码曲线候选 |
| Frozen table wrapper | `284ba1077aeb6cd6df85e51328c0b1a3299a9bf9f113c2ae88e7e1af151e6118` | 表格查询/默认域策略 |
| Frozen table parser | `eb052badd146135bb6022eb12f9206c10cb48a947aca640f95cc52b86f7473a7` | 双线性插值/Clamp 行为 |
| Existing normalized run | `7c96967736f3e1138a4cee78e467c63dda35d43561477ec28a67ef77f285231d` | 同环境双跑一致的 Legacy 输出 |

这些 hash 的 authority 是 `Artifact` 完整性；其内容的模型/科学 authority 仍由后续审批决定。

## 3. 四条 artifact lane

### 3.1 Faithful reproduction

从获批 source edition 建立逐页 equation ledger：原符号、原公式、原假设、原 scenario、原图表/数据 locator、论文明确给出的 numerical procedure，以及所有 paper-unspecified 项。任何符号重命名、SI 归一化或小角近似展开都要同时保留原式和转换证明。

输出只回答“指定来源写了什么”。如果来源没有 bank extension、fallback、分母保护、插值、限幅或数值步长，ledger 必须明确 `unspecified`，不能从 Legacy 代码回填。

### 3.2 Independent verification

从获批 normalized equation ledger 和固定 inputs 生成高精度 expected。producer 不 include/import/link/call target、Legacy 或 Legacy tests；不读取 candidate output；不共享公式 helper。第二位转录者从 source 独立录入，再由结构化 diff 对 equation tree、符号、指数、括号和 sign 做审查。

输出回答“在这些明确 inputs/assumptions/domain 下，这组已核准方程算出什么”。它不决定 runtime object、fallback、telemetry 或 scheduling。

### 3.3 Engineering adaptation

把 source-verified scientific relations 包装成 typed definition、prepared envelope、derivative query、domain/status、fallback disposition、TDCT command、telemetry、Plan binding 与 artifact lineage。任何 bank 投影、表格插值、command limit、sample rate 或降级策略都必须标注 `DeclaredModelChoice` 或 `NumericalPolicy`，不能冒充原论文。

### 3.4 Frozen Legacy behavior

隔离执行 [Legacy source index](../../reference/legacy/source-index.md) 所列的 `user/example_08_cavh_geographic_3dof_custom/components/cavh_glide_range_guidance.hpp` 及其 mission，只记录它实际做了什么。旧 node/provider/config/observable 名、CSV 列序、exception 文本和 silent clamp 是迁移输入，不是 target contract。

四条 lane 各自有不同 `authority_domain`、`source_refs`、`input_hash`、`producer_hash`、`validity` 和 `disposition`。任何 artifact 都不得仅凭同名 case id 在 lane 间替换。

## 4. Source、claim 与 assumption ledger

### 4.1 Claim records

每条 claim 至少包含：

- `claim_id`、简短陈述与 maturity；
- `source_id`、edition、page、equation/figure/table locator；
- 原文符号和 normalized symbol refs；
- 显式假设、推断假设和 paper-unspecified 项；
- 适用 scenario/domain；
- faithful/independent/engineering artifact refs；
- owner、reviewer、approval ref 与状态；
- supersedes/superseded-by 关系。

拟议 claim 分组：

| Claim id | 拟议内容 | 激活前状态 |
| --- | --- | --- |
| `CAVH-CLAIM-001` | 最大升阻比 operating point 与最大航程的关系 | `source_pending` |
| `CAVH-CLAIM-002` | Eq18 simplified steady-glide `gamma*` | `source_pending` |
| `CAVH-CLAIM-003` | Eq17 retaining Mach dependence of `CL*` | `source_pending` |
| `CAVH-CLAIM-004` | TDCT negative-feedback relationship | `source_pending` |
| `CAVH-CLAIM-005` | bank/vertical-lift extension | `decision_pending`，不得预标 paper claim |
| `CAVH-CLAIM-006` | frozen mission/aero/parameter set represents paper scenario | `provenance_pending` |

### 4.2 Assumption categories

Assumption ledger 至少逐项覆盖：

- geometry：平面/球形地球、`r = R + h`、经纬/局部 frame、rotation；
- dynamics：定常滑翔、准平衡、小角度、质量常数、推力、侧向运动；
- atmosphere：`ρ(h)`、`a(h)`、连续性、导数定义与 validity；
- gravity：常量或随高度变化，`g` 在式中的取值位置；
- aero：`CL(α,M)`、`CD(α,M)`、参考面积、系数轴系、正号、smoothness；
- envelope：最大化 `CL/CD` 的变量、区间、唯一性、边界与 branch；
- derivative：partial/total derivative 的固定变量、解析/数值方法；
- command：`γ*`、`γ`、`α*`、bank、gain、限幅和 sampling；
- numerical：precision、rounding、optimizer、interpolation、step 和 denominator policy；
- evidence：source access、digitization、uncertainty、hash、cross-tool 与 platform。

每项取值只能是 `source_explicit`、`source_inferred`、`engineering_choice`、`legacy_behavior`、`unresolved` 之一。`source_inferred` 必须给出推理和 owner approval；`unresolved` 会阻断对应 fact。

## 5. Frozen Legacy 行为审计

### 5.1 数据流

冻结 guidance 当前执行：

1. 从 navigation、atmosphere、gravity、mass 和 aero provider 读取状态；
2. 对速度、声速、密度、重力和质量做最小值保护；
3. 优先相信正的 navigation Mach，否则用 `V/a`；
4. 以高度中心差分求 `dρ/dh` 与 `∂M/∂h`，在海平面将下界 clamp 为 0；
5. 在 alpha 区间均匀采样最大 `L/D`，若 interior 再固定做 80 次 golden-section refinement；
6. 以 Mach 中心差分重新求两次 envelope，得到 `dCL*/dM`；
7. 以 `cos(bank)` 投影 `CL*` 及其 Mach 导数；
8. 计算 Eq18 或 Eq17；Eq17 的 `dCL*/dV` 近零时静默调用 Eq18；
9. Eq17/Eq18 最终三个/两个汇总分母 `Bi` 通过带符号最小绝对值保护；其他 reciprocal 依赖早期输入 clamp 或成功路径假设，没有完整 typed status；
10. 计算 `eγ = γ* - γ`、`δα = kγ eγ` 与 `αcmd = clamp(α* + δα)`；
11. 发布部分 inputs/finals，但不发布全部 `Aij`、分母、dynamic pressure、drag、domain、fallback 或 saturation quality。

### 5.2 当前测试为何不是独立证据

[Legacy source index](../../reference/legacy/source-index.md) 所列的 `tests/test_cavh_glide_range_guidance.cpp` 在 include 生产头文件后，又在同一 translation unit 定义 `paperEq17GammaCommand` 和 `paperEq18GammaCommand`。两者逐项复制生产函数的公式，并使用相同 binary64、相同输入来源和相同默认约定。测试覆盖可以发现部分 wiring/config 退化，却不能发现双方共同的：

- source equation 转录错误；
- 符号、括号、指数或遗漏项；
- `dCL*/dM`、`∂M/∂V`、`∂M/∂h` 的定义错误；
- paper/engineering/Legacy lane 混合；
- denominator clamp、fallback 或 domain policy 不当；
- broad tolerance 同时放过相同偏差。

现有五类调用只覆盖 alpha-sample config、一个 Eq18 抛物线案例、一个 Eq17 Mach-dependent 抛物线案例、一个 banked Eq18 扩展和一个 paper-aero 点。它们不覆盖全部 intermediates、derivative convergence、包线边界/平台/无最优点、域外 table、Eq17 fallback disposition、每个 denominator、non-finite、command saturation 或 source/license drift。

### 5.3 Frozen mission 与资产冲突

冻结 mission 使用 `data/aero_table2d.csv` 和 table wrapper，不使用 `cavh_paper_aero_assets_3dof.hpp`。两份资产必须有不同 identity；单测通过 paper-aero 的一个点不能验证 mission table。当前 mission 还固定：

- `dt = 0.1 s`、duration `5000 s`、RK4；
- reference/earth radius `6,356,766 m`，earth rotation 0；
- initial altitude `80,000 m`、speed `6,800 m/s`、flight-path angle 0、heading `-π/2`；
- guidance 10 Hz、`tdct_gain = 3`、Eq17、bank 0、alpha `[10°,20°]`、121 samples；
- density-gradient step `100 m`、Mach-derivative step `0.05`；
- mass `50,000 kg`、reference area `0.48387 m²`、zero propulsion；
- altitude below `10 km` termination。

这些值只标为 `legacy_behavior`。在 citation/page/table/parameter provenance 关闭前，不能把它们称为论文 scenario、canonical reference 或推荐 target 默认值。

## 6. Normalized symbol 与维度账本

### 6.1 输入符号

| Symbol | Proposed semantic id | Unit/dimension | Domain | Authority 状态 |
| --- | --- | --- | --- | --- |
| `h` | altitude | m | finite；valid atmosphere/earth domain | decision pending |
| `R` | reference earth radius | m | `R > 0` | decision pending |
| `r = R + h` | geocentric radius candidate | m | `r > 0` | legacy transcription |
| `V` | speed magnitude | m/s | `V > 0` | decision pending |
| `γ` | actual flight-path angle | rad | finite；convention pending | decision pending |
| `σ` | bank angle candidate | rad | finite；paper mapping pending | engineering candidate |
| `ρ` | density at `h` | kg/m³ | `ρ > 0` | decision pending |
| `ρ_h = dρ/dh` | density altitude derivative | kg/m⁴ | derivative domain/trust required | decision pending |
| `a` | speed of sound at `h` | m/s | `a > 0` | decision pending |
| `M` | Mach number | 1 | aero/envelope domain | decision pending |
| `M_V = ∂M/∂V` | Mach speed partial | s/m | fixed `h` and atmosphere definition | decision pending |
| `M_h = ∂M/∂h` | Mach altitude partial | 1/m | fixed `V` and atmosphere definition | decision pending |
| `g` | gravity magnitude | m/s² | `g > 0` | decision pending |
| `m` | vehicle mass | kg | `m > 0` | decision pending |
| `S` | aero reference area | m² | `S > 0` | decision pending |
| `α*` | angle at approved `L/D` optimum | rad | envelope alpha domain | decision pending |
| `CL*` | lift coefficient at optimum | 1 | finite; sign/domain policy | decision pending |
| `CD*` | drag coefficient at optimum | 1 | `CD* > 0` | decision pending |
| `CL'_M` | `dCL*/dM` | 1 | branch/domain/trust required | decision pending |
| `kγ` | TDCT gain | pending | sign/unit/sample semantics pending | decision pending |

如果 `M = V/a(h)` 被批准，则候选解析关系是 `M_V = 1/a` 与 `M_h = -V a_h/a²`。这只是定义推论；若 navigation Mach 是独立状态、real-gas model 或另有自变量，不能混用该关系。

### 6.2 派生符号

| Symbol | Candidate definition | Unit | 必须报告的 quality |
| --- | --- | --- | --- |
| `q` | `ρ V² / 2` | Pa | finite/domain |
| `D` | `q S CD*` | N | input lineage、finite |
| `CLv*` | `CL* cos σ` | 1 | bank disposition |
| `CLv'_M` | `CL'_M cos σ` | 1 | bank disposition、derivative trust |
| `CLv'_V` | `CLv'_M M_V` | s/m | condition number、near-zero classification |
| `Bi` | Eq17/Eq18 denominator groups | 1 | value、distance-to-zero、policy |
| `γ*` | steady-glide reference | rad candidate | formula version、finite/domain |
| `eγ` | `γ* - γ` candidate | rad | sign convention |
| `δα` | `kγ eγ` candidate | rad | gain unit、sample semantics |
| `αraw` | `α* + δα` | rad | finite |
| `αcmd` | limited `αraw` | rad | saturation side/delta/limits |

所有 dimensionless 量仍必须有 semantic unit，例如 rad 不能因为在 SI 中 dimensionless 就与 coefficient 互换。

## 7. Legacy-transcribed candidate equations

### 7.1 使用规则

本节只规范冻结代码当前代数，状态是 `legacy_transcription_only`。激活后必须从获批 source edition 独立转录；如果 source-verified 公式不同，应保留本节作为 Legacy difference，而不是修改 expected 去追随 Legacy。

共同定义：

```text
r = R + h
q = ρ V² / 2
D = q S CD*
CLv* = CL* cos(σ)                 [bank extension mapping unresolved]
CLv'_M = CL'_M cos(σ)             [bank extension mapping unresolved]
```

### 7.2 Eq18 simplified candidate

```text
A21 = ρ_h V² / (2 ρ g)
A24 = 2 m / (CLv* ρ S r)
A25 = m V² / (CLv* ρ g S r²)

A31 = ρ_h CLv* V² S r / (4 m g)
A34 = 1 / A24
A35 = V² / (2 g r)

B2 = 1 - A21 + A24 + A25
B3 = 1 - A31 + A34 + A35

γ* = -D / (m g) · (1/B2 + 1/B3)
```

Reference producer 必须分别输出 `r`、`q`、`D`、`CLv*`、六个 `Aij`、`B2`、`B3`、每项 reciprocal、scale `-D/(mg)` 和 `γ*`。只输出最终 `γ*` 不足以定位转录、导数或 denominator 错误。

### 7.3 Eq17 Mach-derivative candidate

先定义：

```text
CLv'_V = CLv'_M M_V
```

候选系数：

```text
A11 = ρ_h CLv* V / (CLv'_V ρ g)
A12 = M_h V / (M_V g)
A13 = 2 CLv* / (CLv'_V V)
A14 = 4 m / (CLv'_V ρ V S r)
A15 = 2 V m / (CLv'_V ρ S g r²)

A21 = ρ_h V² / (2 ρ g)
A22 = CLv'_M M_h V² / (2 CLv* g)
A23 = 1 / A13
A24 = 2 m / (CLv* ρ S r)
A25 = m V² / (CLv* ρ g S r²)

A31 = ρ_h CLv* V² S r / (4 m g)
A32 = CLv'_M M_h ρ V² S r / (4 m g)
A33 = 1 / A14
A34 = 1 / A24
A35 = V² / (2 g r)

B1 = 1 - A11 - A12 + A13 + A14 + A15
B2 = 1 - A21 - A22 + A23 + A24 + A25
B3 = 1 - A31 - A32 + A33 + A34 + A35

γ* = -D / (m g) · (1/B1 + 1/B2 + 1/B3)
```

Reference producer 必须输出 `CLv'_V`、15 个 `Aij`、三个 `Bi`、reciprocal terms、scale 与 `γ*`。`CLv'_V = 0`、任一构成分母为零、任一 `Bi` 接近零或输入 non-finite 都必须先返回 typed result；不得先做符号 clamp 再伪造 finite answer。

### 7.4 TDCT candidate

冻结实现表达为：

```text
eγ = γ* - γ
δα = kγ eγ
αraw = α* + δα
αcmd = clamp(αraw, αmin, αmax)
```

Source equation number、negative-feedback 推导、`kγ` 量纲、角度 convention、sampling 和 limit 是否属于论文仍待 `CAVH-DEC-001/002/010`。Reference bundle 应同时保留 `αraw` 与 `αcmd`，并报告 `not_saturated`、`lower_saturated` 或 `upper_saturated`；不能仅用 clipped value 作为 expected。

## 8. Glide envelope 设计

### 8.1 Prepared envelope record

每个 Mach grid point 至少包含：

- `mach` 与 grid identity；
- alpha search domain 与 units；
- `alpha_star`、`cl_star`、`cd_star`、`lift_to_drag_max`；
- `d_cl_star_d_mach` 与 derivative method/trust；
- optimizer/interpolator id 与 implementation hash；
- objective residual、alpha bracket、estimated optimization/interpolation error；
- optimum class：`interior_unique`、`boundary_lower`、`boundary_upper`、`plateau`、`multiple_branches`、`none`；
- aero-domain status、input data/uncertainty refs；
- configuration-envelope identity 或 perturbation-envelope identity；
- owner approval 与 validity interval。

Configuration envelope 用于 nominal definition/binding；perturbation envelope 用于明确扰动模型。不得在 Monte Carlo、fault 或 sensitivity case 中把一个 envelope 的 `CL*` 与另一 envelope 的 derivative/uncertainty 拼接。

### 8.2 解析抛物线 oracle

为避免把 numerical optimizer 同时当 producer 和 candidate，至少建立一个解析 aero case：

```text
CL(α) = CL0 + CLα α
CD(CL,M) = CD0(M) + K CL²
```

在 `K > 0`、`CD0(M) > 0`、`CLα ≠ 0`、选择正升力极大值分支且解位于 alpha domain 内时：

```text
CL* = sqrt(CD0 / K)
CD* = 2 CD0
(L/D)max = 1 / (2 sqrt(K CD0))
α* = (CL* - CL0) / CLα
dCL*/dM = CD0'(M) / (2 sqrt(K CD0(M)))
```

解析 producer 应直接从 `CD0`、`CD0'`、`K`、`CL0`、`CLα` 计算，不调用 envelope optimizer。Candidate optimizer 必须回报与解析值的误差、边界分类和 objective residual。

### 8.3 Envelope cases

| Case id | 目的 | 必须断言 |
| --- | --- | --- |
| `CAVH-ENV-001` | 常量 `CD0` 的 interior optimum | `alpha*`、`CL*`、`CD*`、`L/Dmax` 解析一致；`dCL*/dM = 0` |
| `CAVH-ENV-002` | 线性 `CD0(M)` 的 interior branch | 全部 envelope 值及解析 `dCL*/dM` |
| `CAVH-ENV-003` | 解析 optimum 正好在 lower boundary | exact boundary class；不得冒充 interior |
| `CAVH-ENV-004` | 解析 optimum 正好在 upper boundary | exact boundary class；不得无声外推 |
| `CAVH-ENV-005` | optimum 落在 alpha domain 外 | approved boundary/failure disposition |
| `CAVH-ENV-006` | plateau 或多个等价 maxima | non-unique quality；derivative 默认 untrusted |
| `CAVH-ENV-007` | 无正 lift 或 `CD <= 0` | stable domain status，无 silent substitute |
| `CAVH-ENV-008` | Mach 位于表格 lower/upper edge | interpolation/domain policy exact |
| `CAVH-ENV-009` | Mach 域外 | reject、explicit clamp 或 explicit extrapolate 中获批的一种，并携带 disposition |
| `CAVH-ENV-010` | 配置与扰动 envelope 混用 | identity/binding failure |

冻结实现的 `alpha_samples = 121`、80 次 refinement、`CD > 1e-12`、`CL > 1e-12` 和 boundary direct return 只作为 Legacy comparison values，不自动成为 target algorithm/tolerance。

## 9. Derivative reference 设计

### 9.1 Derivative identities

每个 derivative record 必须声明 function、independent variable、fixed variables、evaluation point、unit、method、precision、step/bracket、domain、branch identity、uncertainty、condition estimate 与 trust status。

最低覆盖：

- `ρ_h = dρ/dh`；
- `M_V = ∂M/∂V |h`；
- `M_h = ∂M/∂h |V`；
- `CL'_M = dCL*(M)/dM`；
- `CLv'_M` 与 `CLv'_V` 的 chain rule；
- 若 source formula 需要的其他 total/partial derivative。

对指数大气 `ρ = ρ0 exp(-h/H)`，独立解析 truth 是 `ρ_h = -ρ/H`。对 `M = V/a(h)`，批准该 definition 后使用上一节解析 partial。对抛物线 envelope 使用 §8.2 的解析 `dCL*/dM`。

### 9.2 数值 derivative ladder

数值 candidate 至少在一组对称 step ladder 上报告：`Δ`、`Δ/2`、`Δ/4`、每步 estimate、相邻差、相对/绝对 error、observed order、roundoff indicator 与最终 trust。边界处不得把单边差分伪装为中心差分；使用 one-sided stencil 时记录独立 method id 与误差阶。

可接受状态候选：

- `trusted_analytic`；
- `trusted_high_precision`；
- `trusted_converged_numeric`；
- `boundary_one_sided`；
- `branch_discontinuous`；
- `ill_conditioned`；
- `outside_domain`；
- `nonfinite_input`；
- `no_reference`。

只有前三种或 owner 明确允许的降级状态可以进入 Eq17。Eq17 是否在其他状态 fallback 到 Eq18 必须由 Definition 明示，并让 result 携带原失败、fallback target 和 approval ref。

### 9.3 Derivative cases

| Case id | 注入 | 预期 |
| --- | --- | --- |
| `CAVH-DER-001` | 指数大气 interior | analytic `ρ_h` 与 high-precision/numeric convergence |
| `CAVH-DER-002` | constant sound speed | `M_V = 1/a`、`M_h = 0` exact/analytic |
| `CAVH-DER-003` | analytic varying `a(h)` | `M_h` sign、unit 与 value |
| `CAVH-DER-004` | linear `CD0(M)` envelope | analytic `CL'_M` |
| `CAVH-DER-005` | `CL'_M = 0` | Eq17 precondition status，不静默 fallback |
| `CAVH-DER-006` | very small nonzero derivative | condition estimate 与 owner threshold behavior |
| `CAVH-DER-007` | Mach grid boundary | explicit one-sided/domain status |
| `CAVH-DER-008` | branch switch/kink | `branch_discontinuous`，不得 `trusted` |
| `CAVH-DER-009` | step too large/too small | convergence/roundoff failure |
| `CAVH-DER-010` | NaN/Inf sample | `nonfinite_input/output` exact status |

## 10. Eq18、Eq17 与 TDCT case catalog

### 10.1 Case record schema

每个 case 至少包含：

- stable case id、lane、formula/version/source refs；
- literal inputs、semantic ids、units 与 complete input hash；
- assumptions、domain、envelope/derivative refs；
- expected intermediates 与 outputs；
- exact status/fallback/saturation expectation；
- quantity-specific comparator refs；
- producer source/build/runtime hash；
- reviewer/approval refs；
- mutation children 与 expected diagnostic stage/category。

Candidate probe 必须原样回显所有 literal inputs 和绑定 identity。Validator 先比较 input echo，再比较 result；否则 candidate 可以用错误默认值却偶然得到接近输出。

### 10.2 Eq18 cases

| Case id | 设计 | 目的 |
| --- | --- | --- |
| `CAVH-EQ18-001` | 解析抛物线、指数大气、interior optimum、bank 0 | 正常 full-intermediate anchor |
| `CAVH-EQ18-002` | 独立 literal coefficients，不调用 envelope | 隔离 formula 与 envelope |
| `CAVH-EQ18-003` | source/paper worked example | 仅在 source/page/inputs 获批后生成 |
| `CAVH-EQ18-004` | `B2` 接近/等于 0 | denominator typed failure |
| `CAVH-EQ18-005` | `B3` 接近/等于 0 | denominator typed failure |
| `CAVH-EQ18-006` | `CLv*` 接近/等于 0 | precondition failure |
| `CAVH-EQ18-007` | banked extension | engineering lane，不冒充 paper planar case |
| `CAVH-EQ18-008` | non-finite/非物理 inputs | stable field-specific domain status |

### 10.3 Eq17 cases

| Case id | 设计 | 目的 |
| --- | --- | --- |
| `CAVH-EQ17-001` | Mach-dependent analytic parabolic envelope、bank 0 | 正常 full-intermediate anchor |
| `CAVH-EQ17-002` | 独立 literal derivative 与 coefficients | 隔离 formula、envelope 与 derivative producer |
| `CAVH-EQ17-003` | source/paper worked example | 仅在 source/page/inputs 获批后生成 |
| `CAVH-EQ17-004` | `CLv'_V = 0` | strict precondition failure |
| `CAVH-EQ17-005` | `CLv'_V` near zero | condition/fallback disposition |
| `CAVH-EQ17-006` | `B1` singular | denominator 1 typed failure |
| `CAVH-EQ17-007` | `B2` singular | denominator 2 typed failure |
| `CAVH-EQ17-008` | `B3` singular | denominator 3 typed failure |
| `CAVH-EQ17-009` | `M_V = 0` 或 invalid | precondition/domain failure |
| `CAVH-EQ17-010` | derivative untrusted/outside envelope | reject 或 explicitly approved fallback |
| `CAVH-EQ17-011` | banked extension | engineering lane 与 source lane 分离 |
| `CAVH-EQ17-012` | non-finite/非物理 inputs | stable field-specific domain status |

### 10.4 TDCT/command cases

| Case id | 输入关系 | 必须断言 |
| --- | --- | --- |
| `CAVH-TDCT-001` | `γ* = γ` | `eγ = 0`、`δα = 0`、`αraw = α*` |
| `CAVH-TDCT-002` | `γ* > γ` | approved sign 下 correction direction |
| `CAVH-TDCT-003` | `γ* < γ` | approved sign 下 opposite correction |
| `CAVH-TDCT-004` | `kγ = 0` | correction exactly 0 |
| `CAVH-TDCT-005` | lower saturation | raw/limited value、side、delta、limits |
| `CAVH-TDCT-006` | upper saturation | raw/limited value、side、delta、limits |
| `CAVH-TDCT-007` | deg/rad mutation | unit gate rejects before numeric compare |
| `CAVH-TDCT-008` | non-finite command input/output | typed failure，无 clamp-to-finite |

### 10.5 Audit-only numerical seeds

为说明现有测试的可复现形状，本次以 Legacy 测试常数为基础做了独立高精度代数审计：指数大气使用解析 `ρ_h = -ρ/H`，Eq17 则暂时保留 Legacy 的 envelope central-step `CL'_M` 作为单独比较轴。它们状态是 `legacy_audit_seed`，没有 source/page/tolerance/owner approval，绝不能写入 scientific expected：

| Seed | 关键候选输入 | 选定审计结果 |
| --- | --- | --- |
| Eq18 | `ρ=1.225 exp(-h/7200)`、analytic `ρ_h=-ρ/7200`、`h=30000 m`、`V=3000 m/s`、`g=9.81 m/s²`、`m=50000 kg`、`S=100 m²`、`R=6371000 m`、`CL*=0.5`、`CD*=0.04`、bank 0 | `D≈341859.971858155 N`、`B2≈64.7281299933620`、`B3≈3934.47363399037`、`γ*≈-0.01094467466329637` |
| Eq17 | 同一环境，`a=300 m/s`、`M_V=1/a`、`M_h=0`、`CD0=0.02+0.001M`、`K=0.08`、`M=10`、Legacy envelope central `ΔM=0.01`、bank 0 | `CL*≈0.6123724356957945`、`CD*=0.06`、Legacy central-step `CL'_M≈0.01020620740334946`、`γ*≈-0.01769230077327487` |

这些结果只证明本文对冻结代码代数的读取可被另一数值路径复算。Eq17 的解析 derivative 实际为 `CL'_M≈0.01020620726159658`，与上表 Legacy central-step 值并不完全相同；正式案例必须分别保存解析 truth 与 numerical error，不得冻结这个混合 seed 的最终 `γ*`。Legacy finite-difference 结果只进入 behavior/difference lane。

## 11. Independent reference producer

### 11.1 Clean-room 边界

第一 producer 候选为 standalone CPython 工具，只使用标准库 `decimal`、`json`、`hashlib`、`fractions`（需要时）和明确记录的 transcendental 实现/precision policy。它：

- 从 approved normalized case JSON 读取 decimal strings；
- 不扫描或 import 仓库产品/Legacy source；
- 不复用 C++ test helper；
- 不读取 candidate outputs 或以 candidate 反求 inputs；
- 逐 intermediate 输出记录 precision/rounding 下的 canonical decimal string、comparison value、unit、status；
- 在 non-finite/domain/singular case 中输出 typed result，不伪造数值；
- 报告 source、case、producer、runtime 和 output hash。

若 `decimal` 的 transcendental 或 optimizer 需要自建算法，其源代码、收敛证明、test vectors 和误差界进入 producer artifact。优先通过解析案例避免 producer 和 candidate 使用同一种 numerical optimizer。

### 11.2 第二实现与转录审查

至少再有一个独立路径：例如 standalone C++17/Boost.Multiprecision probe、Julia/Mathematica/Matlab 脚本或人工符号推导，由 owner 批准工具和许可证。第二路径不能机械翻译第一份代码。两位转录者独立从同一 source edition 录入 equation AST/ledger，之后比较：

- 运算树、项集合、系数、指数、括号与 sign；
- 原符号到 normalized symbol 的双向 mapping；
- unit/dimension 推导；
- explicit/inferred/engineering assumptions；
- source locator 与 edition hash。

存在差异时 case 保持 `source_pending`，不得选择“更接近 Legacy 输出”的一侧。

### 11.3 Candidate probe

Candidate probe 与 reference producer 分进程、分构建：

1. 读取同一 immutable input artifact；
2. 回显 case/formula/envelope/derivative/definition identity 和全部 literals；
3. 调用未来 target pure kernel；
4. 输出 target intermediates、status、fallback、quality 与 implementation hash；
5. 不可访问 expected artifact path；
6. comparator 在两者完成后才读取 outputs。

构建图和 include/link scan 必须证明 target 零 Legacy dependency、reference 零 target/Legacy dependency。仅靠目录命名不能证明独立。

## 12. Validator 与失败隔离顺序

同一个 production validator 同时执行正常 bundle 和 mutation bundle，固定顺序：

```text
artifact/source integrity
  -> schema and completeness
  -> case/input identity echo
  -> semantic unit/dimension/domain
  -> envelope reference
  -> derivative reference and trust
  -> equation intermediates
  -> denominator/fallback status
  -> final gamma reference
  -> TDCT raw correction
  -> command saturation/quality
  -> full pure kernel
  -> component conformance
  -> open-loop trajectory
  -> closed-loop trajectory/metric
```

上游 stage 失败时，下游结果标为 `blocked_by:<stage>`，而不是继续计算后给出一串二次误差。Closed-loop pass 不能覆盖任何上游 fail；summary 必须分别统计每一 stage 的 `pass`、`fail`、`blocked`、`not_applicable` 与 `pending_authority`。

### 12.1 Stable diagnostic taxonomy

候选稳定 category：

- `Provenance.SourceMismatch`
- `Provenance.UnapprovedEdition`
- `Artifact.IntegrityMismatch`
- `Schema.MissingRequiredFact`
- `Semantic.UnitMismatch`
- `Domain.NonFiniteInput`
- `Domain.NonPhysicalInput`
- `Domain.OutsideEnvelope`
- `Envelope.NoOptimum`
- `Envelope.BoundaryOptimum`
- `Envelope.NonUniqueOptimum`
- `Derivative.Untrusted`
- `Derivative.IllConditioned`
- `Formula.TranscriptionMismatch`
- `Formula.SingularDenominator`
- `Formula.FallbackNotAllowed`
- `Command.SaturationDispositionMissing`
- `Tolerance.PolicyInvalid`
- `Independence.DependencyViolation`
- `Trajectory.BlockedByFormula`

是否最终进入公共 diagnostic contract 由 Architecture owner/ADR 决定。准备阶段只固定需求，不提前新增 schema/type。Human message 可变化；比较使用 category、stage、subject、source/case refs 与 disposition。

### 12.2 Denominator record

每个 denominator 或 reciprocal precondition 至少保存：

- stable term id，例如 `eq17.b1`；
- raw value 与 unit；
- scale/conditioning basis；
- approved exact/absolute/relative threshold ref；
- classification：`regular`、`near_singular`、`singular`、`nonfinite`；
- requested formula；
- actual formula；
- fallback allowed/used、reason 与 approval ref。

禁止通用 `protectDenominator` 在无记录情况下改变 value。若 owner 允许 signed clamp 作为 engineering adaptation，也必须同时保留 unclamped value、clamped value、delta、quality 和下游 validity，且 paper/reference lane 仍返回 singular。

## 13. Tolerance 与不确定度

### 13.1 Exact fields

以下默认 exact，不接受 numeric tolerance：identity/version、source/edition/page/equation ref、license disposition、raw/semantic hash、case/lane/authority、unit/dimension、formula term set、status、domain class、branch、fallback allowed/used、saturation side、artifact completeness、finite/non-finite、producer/build ref。

### 13.2 Numeric policy

每个 numeric comparator 至少声明：

- field/intermediate id 与 case family；
- absolute、relative、ULP、interval containment、residual 或 convergence comparator；
- unit、scale、absolute floor 与 denominator；
- reference precision、rounding 和 uncertainty；
- target representation/rounding/error budget；
- domain/branch/profile；
- approved limit、rationale、owner/version；
- observed error、max location 与 pass/fail。

推荐分层：

| Layer | Reference | 主要比较 |
| --- | --- | --- |
| Algebra identities | rational/Decimal/AST | exact term/identity，能 exact 则 exact |
| Analytic envelope/derivative | closed form/high precision | field-specific error + residual |
| Table/digitized aero | source value + uncertainty interval | interval/uncertainty + interpolation budget |
| Binary64 pure kernel | high-precision rounded reference | per-field abs/rel/ULP budget |
| Numerical derivative | analytic/high precision | error、step convergence、conditioning |
| Trajectory | independent stricter solver | state-family error + dt convergence |

禁止使用：一个全局 epsilon；直接采用 Legacy 测试的 `1e-6`/`1e-9`/`1e-12`；为了让 frozen output 通过而放宽；对 identity/status/hash 使用 tolerance；NaN/Inf 比较静默通过；只比较最终 `gamma*` 或闭环 range；用不同单位量的聚合 max norm 隐藏局部误差。

### 13.3 Aero uncertainty

若 source aero 来自图形 digitization，artifact 必须包含原图/合法 ref、轴标定点、提取工具/版本、逐点 raw coordinates、单位/变换、人工修订、重复 digitization 差异、插值方法和 uncertainty interval。科学 expected 应传播或至少界定该不确定度；不能给图读数据虚构超过来源分辨率的精度。

## 14. Bundle artifact topology

激活后的候选逻辑布局如下；它描述 artifact identity，不在本准备切片创建空文件：

```text
fixtures/ref-cavh-formula/
  fixture-manifest.json
  sources/
    citation-ledger.json
    license-ledger.json
    source-index.json
  equations/
    source-equations.json
    normalized-equations.json
    symbol-ledger.json
    assumption-ledger.json
    transcription-review.json
  inputs/
    aero-assets/
    environment-cases.json
    envelope-cases.json
    formula-cases.json
    tdct-cases.json
  reference/
    producer-a/
    producer-b/
    expected/
  candidates/
    target-probe/
    legacy-capture/
  mutations/
  reports/
    validation-report.json
    tolerance-report.json
    difference-report.json
    independence-report.json
    replay-report.json
  artifact-index.json
```

每个 index entry 至少有 logical path、media/schema、bytes、raw SHA-256、可选 canonical SHA-256、producer、input refs、authority、validity、license 和 supersession。External restricted source 可用受控 ref + verified hash 表示，但 validator 必须能区分 `available_and_verified`、`externally_verified`、`unavailable`，后两者不能被误报为本地完整。

## 15. Legacy semantic comparison 与迁移 disposition

### 15.1 当前候选分类

| Disposition | 候选事实 | 约束 |
| --- | --- | --- |
| Preserve after science verification | 最大 `L/D` operating-point 概念、Eq17/Eq18/TDCT 的语义角色 | 只有 source/independent evidence 通过后，不保留旧实现形状 |
| Preserve as Legacy fact | 冻结 mission/config、旧双跑终止、observable values | 只在 Legacy lane，可用于差异审计 |
| Retire | node/provider/lookup/config 名、mutable query telemetry、CSV 列序、exception 文本、固定 80 次 loop | accidental implementation，不进入 target truth |
| NeedsDecision | silent input clamps、denominator clamp、Eq17→Eq18 fallback、table clamp、finite-difference steps | 未经 owner 不得 Preserve/Fix |
| NeedsDecision | mission CSV、paper-aero curves、mass/area/earth/atmosphere 参数、bank extension | provenance/model choice 未关闭 |
| Fix | 当前无自动批准项 | 需要 defect id、independent reproduction、impact、owner decision 和 replacement evidence |

### 15.2 Fresh closed-loop capture

现有 R0-LEG-001 双跑记录了 CAVH mission 在固定 Windows 工具链下于约 `t=2410.7 s` 因高度低于 `10 km` 提前终止，raw CSV hash 两次一致；大体量 raw CSV bytes 未提交到仓库。这足以证明一次冻结行为复现，不足以比较 paper/target truth。

只有 formula bundle 的 source、equation、envelope、derivative、domain 与 TDCT stage 通过后才 fresh capture：

1. 从固定 ZIP 解压到新隔离 workspace；
2. 核对 archive/mission/guidance/aero/parser/toolchain hashes；
3. 普通 CLI 双跑，保存 command、cwd、environment、stdout/stderr、raw CSV/summary；
4. 生成 semantic sidecar，不依赖 24 个旧 node、145 个稳定字段或 CSV 顺序；
5. 对齐相同 physical inputs、sample/terminal semantics；
6. 每项差异标 `Preserve/Fix/Retire/NeedsDecision` 与 owner ref；
7. target/reference 不 include/link/run Legacy；外部 comparator 只消费 artifacts。

Closed-loop comparison 是最后一层 regression/effect evidence，不是 formula expected 的来源。

## 16. Mutation matrix

生产 validator 必须用真实 parser、schema、reference/candidate comparator 和 diagnostic path 执行下列 mutation；不得在测试里直接断言另一个简化函数：

| Mutation id | 改动 | 首个必须失败的 stage |
| --- | --- | --- |
| `CAVH-MUT-001` | 错 DOI、edition、page、source/license ref 或 source hash | artifact/source integrity |
| `CAVH-MUT-002` | 交换 Eq17/Eq18 label 或公式版本 | formula identity |
| `CAVH-MUT-003` | 翻转一个 source term/sign/括号/指数 | equation/intermediate |
| `CAVH-MUT-004` | 把 rad 当 deg、m 当 km、导数量纲互换 | semantic unit/dimension |
| `CAVH-MUT-005` | 删除 `Aij`，但调整 final tolerance 企图通过 | intermediate completeness |
| `CAVH-MUT-006` | `ρ_h` sign 反转或未声明 one-sided boundary | derivative reference |
| `CAVH-MUT-007` | 以 navigation Mach 求 envelope、以 `V/a` 求 derivative，identity 不同 | case/input identity |
| `CAVH-MUT-008` | `M_V` 与 `M_h` 自变量或固定变量错误 | derivative definition |
| `CAVH-MUT-009` | 最大 `CL` 代替最大 `L/D` | envelope objective |
| `CAVH-MUT-010` | boundary/plateau/no-optimum 标为 trusted interior | envelope quality |
| `CAVH-MUT-011` | table 域外 silent clamp/extrapolate | aero/envelope domain |
| `CAVH-MUT-012` | branch/kink 的 `CL'_M` 标为 converged | derivative trust |
| `CAVH-MUT-013` | bank projection漏用、重复或放错位置 | equation intermediate/lane |
| `CAVH-MUT-014` | Eq17 zero/near-zero derivative silent fallback Eq18 | fallback policy |
| `CAVH-MUT-015` | 任一 `Bi` 或 reciprocal precondition silent signed clamp | denominator status |
| `CAVH-MUT-016` | NaN/Inf、非正 mass/density/gravity/area/radius clamp 为最小值 | domain/finite |
| `CAVH-MUT-017` | TDCT error sign、gain unit 或 angle unit 反转 | TDCT formula/unit |
| `CAVH-MUT-018` | command saturation 丢失 raw value/side/quality | command disposition |
| `CAVH-MUT-019` | reference import target/Legacy，或 expected 与 candidate 共享 helper | independence graph |
| `CAVH-MUT-020` | source/input/producer/artifact 漂移、宽 tolerance 或 closed-loop pass 掩盖上游 fail | integrity/tolerance/stage order |

每个 failure record 至少包含 mutation id、stage、stable category、case/field/term、source/input refs、expected disposition、actual status 与 validator hash。所有 mutation 都应在其最早可判定 stage 失败。

## 17. 实施切片与 gate

### Slice 0：依赖、来源与 authority closure

- 三个 backlog 依赖由有权 reviewer 关闭；
- 指派 assignee/reviewer，合法激活任务；
- 获取并固定合法 source edition；
- 关闭 `CAVH-DEC-001`–`CAVH-DEC-012`；
- 新公共 contract/schema 先走 ADR。

### Slice 1：faithful reproduction

- citation/license/source index；
- 原式、原符号、page/equation locator；
- normalized equation/symbol/assumption ledger；
- 双人独立转录与差异关闭；
- source/normalized mutation。

### Slice 2：independent formula oracle

- 解析 envelope 与 derivative cases；
- Eq18/Eq17 literal 与 integrated cases；
- 全 intermediate、domain、denominator 与 fallback；
- producer A/B cross-check；
- artifact hashes 与 field tolerance。

### Slice 3：engineering wrapper 与 TDCT

- typed definition/binding；
- prepared configuration/perturbation envelope；
- derivative query trust；
- TDCT/raw command/limits/quality；
- candidate probe、production validator 与 mutation matrix。

### Slice 4：component、trajectory 与 Legacy comparison

- pure kernel/component conformance；
- short/open-loop cases；
- formula gate 通过后 closed-loop；
- fresh Legacy 双跑和 semantic mapping；
- difference/tolerance/convergence reports。

### Slice 5：bundle closure 与 handoff

- manifest 从 `specification_only` 转 approved executable 状态；
- artifact index、replay、independence、Windows/Linux CI；
- owner 具名审查；
- 后续 R1 kernel、R2 Compiler/Plan、R3 Session 消费同一 immutable bundle；
- runtime 尚未实现的 assertions 保持 `target_pending`，不在 R0 伪造。

每个 slice 单独自审、验证并提交 Git；上一个 slice 的 evidence 或 owner gate 未关闭时不进入下一个 slice。

## 18. 完整退出检查

- 主来源的 citation、edition、license、source hash、page/equation map 可复核；
- faithful、independent、engineering、Legacy 四条 lane 没有 authority 或 output 偷换；
- 每条 claim 和 assumption 有状态、source、owner 与 approval；
- Eq17/Eq18 原式与 normalized equation 通过双人转录；
- 所有 inputs、`Aij`、`Bi`、reciprocal、scale、`γ*`、TDCT 与 command intermediates 完整；
- 解析/高精度 envelope 和 derivative 独立于 candidate optimizer/query；
- boundary、plateau、no-optimum、domain、branch、conditioning 与 uncertainty 显式；
- Eq17 fallback 与 denominator policy 由 Definition 明示，reference lane 不 silent clamp；
- table/paper-aero/mission 参数逐项有来源或保持 `NeedsDecision`；
- field-specific tolerance 有误差预算，identity/status/hash exact；
- reference producer 与 target/Legacy 零共享，candidate 不读取 expected；
- 20 项 mutation 在最早 stage 由 production validator 拒绝；
- closed-loop 只在全部 formula gates 通过后执行；
- artifact index、raw/canonical hashes、双跑、replay、difference、independence report 完整；
- Debug/Release、Windows/Linux hosted CI、repository verification、`git diff --check` 通过；
- Scientific Authority、Validation Lead、Runtime Numerics Lead、Model SDK Lead、Architecture/Artifact owner 具名审查。

## 19. 准备设计自审

- 来源审查：只把 AIAA DOI/作者成果页和 Springer DOI 当 metadata pointers；未把无法核验的原文、摘要或方程编号写成 truth。
- 公式审查：完整记录冻结 Eq17/Eq18/TDCT 代数并显式标 `legacy_transcription_only`；正式 expected 必须从获批 source 重新转录。
- 独立性审查：指出现有测试 helper 的循环校验，设计 clean-room producer、第二转录者、分进程 candidate 和 dependency scan。
- 数值审查：用解析抛物线、指数大气、step ladder、condition/status 和逐 denominator cases 隔离 optimizer/derivative/formula 错误。
- 领域审查：把 silent clamp、boundary optimum、table clamp、zero derivative fallback、bank extension、saturation 全部转为明确 decision/status，不预选 policy。
- 证据审查：定义完整 artifact topology、hash、input echo、stage-order、mutation 和 replay；closed-loop 不再承担公式真值角色。
- 架构审查：准备设计不新增公共 schema/type，不修改 R1–R3 产品树，不让 target 或 reference 链接 Legacy。
- 状态审查：fixture、backlog、依赖和 owner 状态均未修改；本文是 dependency-blocked preparation，不是 Scientific Authority approval 或任务完成声明。
