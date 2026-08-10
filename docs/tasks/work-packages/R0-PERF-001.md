# R0-PERF-001：容量、确定性与性能测量准备包

- 状态：Prepared — dependency blocked
- Backlog 状态：`planned`（本准备切片不激活任务）
- Assignee：未指派
- Owner role：Runtime Numerics Lead
- 协作角色：Architecture Lead、Validation Lead、Scientific Authority、Compiler Lead、Evidence Lead
- 准备人：Codex
- 准备日期：2026-08-10
- 依赖：`R0-GOV-001`、`R0-SCI-002`
- 关联 gate：R0 / G1；预算决策最迟 G3

## 准备结论

仓库当前没有 benchmark executable、benchmark manifest、hardware profile、raw sample artifact 或 baseline report，也没有可测量的 ExecutionPlan、StateLayout、Session、RunOutcome、observation queue 或 per-step hot path。当前产品只是 C++17 interface-module skeleton、CLI self-check 和 R0 conformance tests；测量它们只能得到编译、进程启动或脚本开销，不能为 R2/R3 layout/arena/queue 决策提供可信 workload 或 budget。

任务的两个依赖均未关闭：`R0-GOV-001` 为 `review`，候选工具链 ADR 尚未接受且 hosted CI 尚无运行证据；`R0-SCI-002` 为 `planned`，`REF-MINIMAL-3DOF-001` 仍是 `specification_only`，没有可执行 scientific workload。backlog 还引用 `06 §25`，但 [06 蓝图](../../../design-notes/gnczmkn-architecture-roadmap/06-simulation-kernel-time-and-lifecycle.md)当前只到 §22；准备阶段不能擅自猜测它应指向 §16、§18、§21、§22 或其他版本。

现有 Legacy 双跑能够证明固定旧环境下 normalized semantic output 一致，却不能作为 target 性能基线：同一 YYZ 任务的两次捕获分别为 `0.168 s` 与 `0.040 s`，CAVH 为 `1.799 s` 与 `1.773 s`，采样没有 warm-up、affinity、thermal、power、timer-overhead 或重复统计控制。wall time 也被明确从 Legacy normalized hash 中排除。这个事实进一步要求把科学正确性、执行确定性、容量、性能分布和实时资格分开。

本准备包定义 12 项 authority decision、D0–D3 候选可判定语义、workload/metric/budget 维度、受控硬件与构建 profile、raw-sample 证据链、实施切片和 20 项 mutation。详细设计见 [容量、确定性与性能基线设计](../../quality/performance-determinism-baseline-design.md)。本切片不产生 baseline 数值、不新增 benchmark target、不修改 backlog/schema/CI/产品/Legacy，也不构成 `R0-PERF-001` 完成或任何性能、实时、D0–D3 达成声明。

## 权威输入

- [`R0-PERF-001` backlog entry](../backlog.json)：deliverable、acceptance、owner、依赖和 architecture refs 的唯一任务来源；
- [03 数学与数值基础](../../../design-notes/gnczmkn-architecture-roadmap/03-mathematics-and-numerical-foundation.md) §20、§22、§23：D0–D3、验证 artifact 与性能原则；
- [06 Session 内核](../../../design-notes/gnczmkn-architecture-roadmap/06-simulation-kernel-time-and-lifecycle.md) §15–§22：并发隔离、确定性、资源预算、RunOutcome、测试和完成定义；backlog 的 `§25` 引用待 owner 纠正；
- [`R0-GOV-001`](R0-GOV-001.md)、[Proposed ADR-0009](../../adr/0009-accountable-roles-and-candidate-toolchain.md)、[候选工具链矩阵](../../governance/toolchain-support-matrix.json)和 [CI workflow](../../../.github/workflows/ci.yml)；
- [`R0-SCI-002`](R0-SCI-002.md)、[`REF-MINIMAL-3DOF-001` manifest](../../../fixtures/ref-minimal-3dof/fixture-manifest.json)和 [最小 3DoF reference 设计](../../quality/minimal-3dof-reference-design.md)；
- [开放决策 D-007/D-009](../../handoff/open-decisions.md)、[风险 RK-009](../../handoff/risk-register.md)、[验收矩阵](../../quality/acceptance-matrix.md)和 [release gates](../../handoff/release-gates.md)；
- `CMakeLists.txt`、`CMakePresets.json`、`project-manifest.json`、当前 build flags 与 R0/Legacy evidence；
- `AGENTS.md` 的 R0 gate、ADR、owner、证据和 Legacy 只读纪律。

## 当前测量面与缺口

| Surface | 已有事实 | 不能由它证明 | 激活后必须补齐 |
| --- | --- | --- | --- |
| Product graph | 9 个 interface modules、skeleton CLI/test | Compiler/Plan/Session 的容量或热路径 | 可执行 R2/R3 fixture 与阶段计时点 |
| CMake/Release | MSVC `/O2 /Ob2 /DNDEBUG /WX` 候选构建 | 显式 FP mode、D2/D3 或稳定 binary profile | compiler/flags/link/dependency/binary hash 与 FP environment |
| CI | 两个固定 OS image family 的 Release build/test workflow | 固定硬件性能或成功 hosted evidence | push 后运行证据；性能 gate 使用受控 dedicated profile |
| Toolchain matrix | Windows/MSVC 与 Ubuntu/GCC 为 `candidate-primary` | 已支持、跨构建稳定或可比较性能 | ADR acceptance、exact per-run identity 与 qualification evidence |
| D0–D3 prose | 等级目标与高层约束 | 逐字段 comparator、normalization、重复次数或 achieved level | machine-readable requested/achieved matrix 与 downgrade reason |
| SCI-002 | 最小 3DoF case 设计 | executable workload 或 target timing | approved bundle、candidate probe 和 correctness gate |
| Legacy evidence | 双跑 normalized hashes 一致 | target D1/D2/D3 或性能分布 | 仅作 Legacy lane；target 使用新 artifacts |
| Local workstation | 可探测 CPU/OS/RAM/timer/power seed | approved baseline、跨机器 comparability | owner-assigned profile id、完整 capture、稳定性与运行协议 |
| Budgets | D-009 默认“先记录测量，不承诺硬实时” | latency/capacity threshold 已获批 | consumer/workload-derived observation/guardrail/qualification budgets |
| Architecture ref | `03 §20`、`03 §23` 有效；`06 §25` 不存在 | authoritative replacement section | Runtime Numerics Lead + Architecture Lead 纠正并记录 |

## 本次审计锚点

- backlog raw SHA-256：`8627b65f1d655f09043833eb23965c3bf72772e554eedc9e920429d131f3a06c`
- 03 蓝图 raw SHA-256：`190f42ed8b81c0221069987ecae124c8e6da04aa0fa799e67a05ae3e2cffbea8`
- 06 蓝图 raw SHA-256：`4325e5c71e5e758d42afb7cf2a8d50f0573c3ab0fbde4f06ec7ab2e738e92165`
- toolchain matrix raw SHA-256：`1923a2f4045e215638c7bc19518cd6d2be356769527be8f6e4cacdc4fefc1bbb`
- CI workflow raw SHA-256：`33a6afa5b2adfbcb4143b142fd56cff2ecb10e2625c2f9f79947fbd4e94198e2`
- CMake presets raw SHA-256：`721b850065c230644d8e9ab1986608e1a62cee1404c39d82df6677d5f9c7dd0e`
- CMake target graph raw SHA-256：`23cb5111dae0da437747742db6432906c0f84c2f60437b6dacd450302c3e783d`
- `R0-SCI-002` work package raw SHA-256：`fb5cc574fb298ee15a2a935f768a2584bf4d4f0aa84938cf9950e3d17382959a`
- minimal 3DoF design raw SHA-256：`0e2d0131ea3874ea49cfd4fdbc6f8232009c4cc3231826bf3c4fc45933f1f8e2`
- Legacy mission report raw SHA-256：`a9f3c3c9066346430b50b607429c39b41074b8ce6e5b443e354536db8d1d0a1d`

这些 hash 只固定准备审计读取的 bytes。build 目录和本机探测不是受版本控制的 authority；不得从它们反推支持承诺或预算。

## 五类结论不得混合

| 类别 | 回答的问题 | 核心证据 | 禁止偷换 |
| --- | --- | --- | --- |
| Scientific correctness | 结果在模型/数值误差内正确吗 | oracle、收敛、逐字段 tolerance | “更快”不能补偿错误 |
| Determinism | 相同声明边界下结果是否按 D0–D3 可复现 | semantic outputs、ordering、RNG、status、hash | wall-time 接近不是确定性 |
| Capacity | 明确资源上限内能否拒绝/承载且无 silent loss | size vector、high-watermark、typed overflow | 一次小案例成功不是容量 |
| Performance | 受控 profile 下时间、吞吐、内存、分配的分布 | raw samples、统计、环境、binary hash | 单次秒表不是 baseline |
| Real-time qualification | deadline、调度、内存、阻塞是否有平台资格 | 独立认证 profile 与 worst-case evidence | soft pacing 或高平均吞吐不是硬实时 |

## 激活前必须关闭的决策账本

| Decision id | 必须决定的内容 | 最低 owner/reviewer |
| --- | --- | --- |
| `PERF-DEC-001` | 纠正不存在的 `06 §25` 引用，固定真正 authoritative section/version/hash | Runtime Numerics Lead + Architecture Lead |
| `PERF-DEC-002` | 首要 consumer、mission scale、R2 graph 与 R3 state/observation/queue workload points | Product Owner + Runtime Numerics Lead + Compiler Lead |
| `PERF-DEC-003` | D0–D3 的逐字段 exact/tolerance/normalization 边界、默认 requested level 与 downgrade policy | Runtime Numerics Lead + Scientific Authority + Validation Lead |
| `PERF-DEC-004` | baseline OS/CPU/build/compiler/FP/thread profile；candidate-primary 与 performance-qualified 分离 | Runtime Numerics Lead + Architecture Lead |
| `PERF-DEC-005` | measurement stages、clock/counter、instrumentation、allocation/memory/queue metrics 与 overhead policy | Runtime Numerics Lead |
| `PERF-DEC-006` | warm-up、process model、affinity、sample count/duration、randomization、outlier/statistics/CI 方法 | Runtime Numerics Lead + Validation Lead |
| `PERF-DEC-007` | budget class：observation-only、candidate guardrail、qualification；阈值、absolute floor、ratio 与批准流程 | Product Owner + Runtime Numerics Lead |
| `PERF-DEC-008` | capacity dimensions、limit/limit+1 cases、overflow/backpressure/drop/typed failure 和恢复语义 | Runtime Numerics Lead + Architecture Lead |
| `PERF-DEC-009` | wall metrics 与物理 state/semantic hash 隔离；RunOutcome/Artifact 中记录哪些 summary | Runtime Numerics Lead + Evidence Lead |
| `PERF-DEC-010` | manifest/profile/raw sample/report schema、canonical/raw hash、artifact retention 与敏感 host 信息处理 | Architecture Lead + Evidence Lead + Validation Lead |
| `PERF-DEC-011` | hosted CI 只做 smoke/determinism 还是使用 dedicated runner 执行性能 gate；runner drift policy | Product Owner + Runtime Numerics Lead + Validation Lead |
| `PERF-DEC-012` | benchmark harness/runner 的依赖、license、build boundary、optimizer guard 和与 target/Legacy 的隔离 | Architecture Lead + Runtime Numerics Lead + Product Owner |

任何 budget、D-level、hardware profile 或 workload point 都不能由当前本机最快一次、Legacy 秒数、hosted runner 波动或 benchmark 跑完后再选择的阈值自动关闭。

## 依赖关闭后的实施切片

1. 关闭两个依赖、`PERF-DEC-001`–`PERF-DEC-012`，指派 assignee/reviewer 并合法激活 backlog；
2. 建立 machine-valid workload、build、hardware/environment、measurement 与 determinism profile schemas；公共 contract 变更先走 ADR；
3. 先实现 calibration 和 correctness-gated minimal 3DoF workload，证明 timer、raw sample、output hash 与失败路径；
4. 为 R2 Compiler/Plan 建立 graph cardinality、prepare memory、compile latency、plan image size 与 rejection workload；
5. 为 R3 Session 建立 state/cell/obligation/rate/solver/observation/queue/cardinality 轴和 hot-path allocation/latency workload；
6. 在受控 Release profile 上执行 warm-up、独立 process repetitions、raw samples 和 environment capture；
7. 对同 profile 生成 measurement baseline；先 observation-only，owner 批准后才能升级 guardrail/qualification；
8. 执行 D0–D3 请求/达成矩阵、new-process/reset/order/thread/compiler/FP mutations 和 downgrade evidence；
9. 生成 artifact index、benchmark report、determinism report、capacity report、instrumentation-overhead report 与 replay command；
10. Runtime Numerics Lead、Scientific Authority、Validation Lead、Compiler Lead、Architecture/Evidence owner 具名审查后转 `review`。

## 本准备切片保持零修改

- backlog、任务状态、assignee、公共 schema 与 ADR 状态；
- `.github/workflows/ci.yml`、CMake target/preset、compiler flags 和 dependency lock；
- `framework/`、`packages/`、`adapters/`、`apps/`、`user/` 产品树；
- `fixtures/ref-minimal-3dof/` 与其他 scientific expected；
- `reference/legacy/` source、evidence 和 timing records；
- benchmark executable、hardware/baseline manifest、预算值或 D-level achieved 记录；
- OS power、affinity、firmware、virtualization 或本机设置。

## 必测失败路径

| Mutation | 注入 | 预期拒绝 |
| --- | --- | --- |
| `PERF-MUT-001` | 继续引用不存在的 `06 §25` 作为已解析 authority | architecture-ref gate |
| `PERF-MUT-002` | workload source/input/size vector 缺失或漂移 | workload identity/integrity gate |
| `PERF-MUT-003` | Debug、未记录 flags 或不同 binary 冒充同一 Release baseline | build-profile gate |
| `PERF-MUT-004` | hardware/OS/power/virtualization/affinity profile 不完整 | environment completeness gate |
| `PERF-MUT-005` | 不同 hardware profile 的样本直接做回归结论 | comparability gate |
| `PERF-MUT-006` | hosted runner patch/hardware 漂移却复用稳定 performance baseline | runner-identity gate |
| `PERF-MUT-007` | 非单调 clock、timer resolution 不足或 measurement overhead 未报告 | instrumentation gate |
| `PERF-MUT-008` | warm-up、setup、I/O、artifact flush 混入 per-step hot path | measurement-boundary gate |
| `PERF-MUT-009` | 只保存 median/p95，不保存 raw sample 与 run identity | evidence completeness gate |
| `PERF-MUT-010` | 跑完后删除 outlier 或选择最有利阈值 | predeclared-statistics gate |
| `PERF-MUT-011` | benchmark 输出未消费，被 optimizer 消除 | workload-validity/output-hash gate |
| `PERF-MUT-012` | wall time、host path 或随机 GUID 进入 physical semantic hash | determinism-boundary gate |
| `PERF-MUT-013` | case RNG 由执行序号派生，重排后样本改变 | RNG identity/D1 gate |
| `PERF-MUT-014` | 对 status、order、tick、identity/hash 使用 numeric tolerance | determinism comparator gate |
| `PERF-MUT-015` | fast-math/未知 FP environment 仍声称 D2/D3 | determinism-profile gate |
| `PERF-MUT-016` | D3 在未固定 platform/libm/thread/fenv 时标 achieved | D3 evidence gate |
| `PERF-MUT-017` | capacity limit+1 silent drop、clamp 或无限增长 | capacity/failure gate |
| `PERF-MUT-018` | instrumentation 改变模型 output、schedule 或 allocation behavior 且未量化 | probe-effect gate |
| `PERF-MUT-019` | 当前 skeleton/CTest 或 Legacy elapsed seconds 作为 R2/R3 budget | workload-authority gate |
| `PERF-MUT-020` | scientific/hash fail 被更快 wall time 或聚合 score 掩盖 | stage-order/completeness gate |

## 激活前置条件

1. `R0-GOV-001` 与 `R0-SCI-002` 由有权 reviewer 关闭为 `done`；
2. Runtime Numerics Lead 与 Architecture Lead 关闭错误 architecture ref 和 `PERF-DEC-001`–`PERF-DEC-012`；
3. baseline/runner profile 不再只是 candidate，相关 ADR 与角色签字齐全；
4. minimal 3DoF correctness fixture 可执行，benchmark 不以错误计算换取速度；
5. assignee/reviewer 已指派，backlog 状态转换满足治理规则；
6. 未实现的 R2/R3 workload 可保持 `target_pending`，但不得用 skeleton 或 Legacy 伪造观测。

## 完整验收证据

- benchmark manifest、hardware/build/environment/workload/measurement profiles machine-valid 且完整 hash 闭合；
- D0–D3 requested/achieved/downgrade 逐 profile、逐 comparand 可判定；wall metrics 不进入物理确定性；
- SCI-002 correctness gate 先通过，benchmark output 可观察且不被 optimizer 消除；
- R2 workload 覆盖 graph/edge/cell/field/rate/asset cardinality、compile/prepare、plan size 和 limit+1；
- R3 workload 覆盖 state/obligation/solver eval/observation/queue/event/cardinality、step latency、throughput、allocation、memory 和 high-watermark；
- setup/prepare/init/hot-step/finalize/I/O 分段，raw samples、warm-up、timer overhead、process/affinity/power/thermal 记录完整；
- performance comparison 只在批准的 comparable profile 内执行；hosted runner drift 不触发虚假 regression；
- budgets 带 workload/profile/metric、class、owner、阈值、absolute floor、ratio、统计和版本；默认不声称硬实时；
- capacity limit 与 limit+1 返回 approved typed outcome，无 silent drop/clamp/unbounded growth；
- 正常 cases 与 20 项 mutation 由同一 production validator 执行；
- source/build/binary/input/environment/raw/report/artifact index 可重放并保存 full SHA-256；
- Windows/MSVC、Ubuntu/GCC 的批准 deterministic matrix 与 dedicated performance profile evidence 完整；
- repository verification、Debug/Release correctness、`git diff --check` 和 owner 具名审查通过。

## 准备切片审查记录

- 实现自审：Codex，2026-08-10；结论为“已把不可测现状、错误 architecture ref、D0–D3 可判定边界、workload/metric/profile/budget 设计和 20 项 mutation 收敛为可领取准备包，未产生虚假 baseline”。本自审不替代 owner review。
- 状态审查：`R0-PERF-001` 仍为 `planned`、assignee 为空；两个依赖未 `done`；D-009 仍 Open。
- 产品审查：当前只有 skeleton，无 R2/R3 hot path；因此没有新增 benchmark target 或发布无意义秒数。
- 确定性审查：Legacy normalized hash 一致只保留为旧行为事实；wall-time 波动不进入 D-level truth。
- 环境审查：本机 AMD Ryzen 9 9950X、16C/32T、约 61.647 GiB、Windows 11 x64 build 26200、高性能电源、hypervisor present 和 10 MHz high-resolution stopwatch 仅是 audit seed，不是 approved profile。
- 构建审查：候选 Release 使用 `/O2 /Ob2 /DNDEBUG /WX`，但未显式固定 FP mode；没有声称 D2/D3。
- CI 审查：workflow 没有 benchmark job，且 hosted evidence pending；滚动 hosted hardware 不作为稳定性能 gate。
- 边界审查：backlog、schema、CI、CMake、产品和 Legacy 均未修改；本准备包不激活任务、不分配人、不越过 R0 gate。
