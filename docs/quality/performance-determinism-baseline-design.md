# R0-PERF-001：容量、确定性与性能基线设计

## 1. 目的与非目标

本文定义 [`R0-PERF-001`](../tasks/backlog.json) 在依赖和 owner decision 关闭后应如何建立 benchmark manifest、D0–D3 determinism target、baseline hardware/build profile、capacity workload、raw measurement evidence 与 budget。它服务于未来 R2 Compiler/Plan 和 R3 Session/layout/arena/queue 选择，不在 R0 准备阶段制造尚未实现的 target runtime 或性能承诺。

本文不做以下事情：

- 不把当前 skeleton CLI、CTest、PowerShell validator 或构建耗时称为 simulation baseline；
- 不把 Frozen Legacy elapsed seconds 称为 target benchmark；
- 不根据本机单次最快结果制定 budget；
- 不把固定 OS image family 的 hosted runner 当固定性能硬件；
- 不把平均吞吐、soft pacing 或低延迟样本称为硬实时资格；
- 不把 D0–D3 prose 自动升级为 machine contract；
- 不修改 backlog、schema、CMake、CI、产品模块或 Legacy；
- 不声称 `R0-PERF-001`、D-009、G1 或 G3 已关闭。

所有 profile、workload point、comparator 与 budget 在 Runtime Numerics Lead 等 owner 签署前均为 `proposal_pending_owner`。没有 raw samples、完整环境和 output correctness 的秒数为 `invalid_measurement`，不是低成熟度 baseline。

## 2. Governing inputs 与审计事实

### 2.1 任务与蓝图

- [`R0-PERF-001` backlog entry](../tasks/backlog.json)要求 benchmark manifest、D0–D3 targets、baseline hardware profile 和 baseline benchmark report；依赖 `R0-GOV-001` 与 `R0-SCI-002`；
- [03 数学与数值基础](../../design-notes/gnczmkn-architecture-roadmap/03-mathematics-and-numerical-foundation.md) §20 定义 D0–D3，§22 要求验证 artifact 记录平台/编译选项/性能/确定性，§23 要求热路径预编译/预分配并让 benchmark 记录数据规模、CPU、编译选项和 determinism；
- [06 Session 内核](../../design-notes/gnczmkn-architecture-roadmap/06-simulation-kernel-time-and-lifecycle.md) §15–§18 定义并发隔离、确定性、实时模式与资源预算，§19 要求 RunOutcome 记录 requested/achieved determinism 和 resource summary，§21 给出 determinism replay test；
- backlog 引用的 `06 §25` 在当前 source 中不存在；06 当前最后一节是 §22。未经 Runtime Numerics Lead 与 Architecture Lead 决策，不能把一个猜测的替代章节写回 task truth；
- [开放决策](../handoff/open-decisions.md) D-007 要求 state/slot/arena 物理布局等待 benchmark，D-009 的默认约束是“先记录测量，不承诺硬实时”；
- [风险登记](../handoff/risk-register.md) RK-009 明确要求避免到 R3 后才发现 layout/arena 不可承载；
- [验收矩阵](acceptance-matrix.md)把多 Session/vector benchmark 和 realtime consumer 放在更晚 gate，R0 不提前宣称这些能力。

### 2.2 Source integrity anchors

| Artifact | Raw SHA-256 | 用途 |
| --- | --- | --- |
| Backlog | `8627b65f1d655f09043833eb23965c3bf72772e554eedc9e920429d131f3a06c` | task authority 与错误 `06 §25` ref |
| Blueprint 03 | `190f42ed8b81c0221069987ecae124c8e6da04aa0fa799e67a05ae3e2cffbea8` | D0–D3、verification、performance principles |
| Blueprint 06 | `4325e5c71e5e758d42afb7cf2a8d50f0573c3ab0fbde4f06ec7ab2e738e92165` | Session determinism/resource/RunOutcome |
| Toolchain matrix | `1923a2f4045e215638c7bc19518cd6d2be356769527be8f6e4cacdc4fefc1bbb` | candidate build profiles |
| CI workflow | `33a6afa5b2adfbcb4143b142fd56cff2ecb10e2625c2f9f79947fbd4e94198e2` | fixed image-family correctness jobs |
| CMake presets | `721b850065c230644d8e9ab1986608e1a62cee1404c39d82df6677d5f9c7dd0e` | Debug/Release configure identity |
| CMake target graph | `23cb5111dae0da437747742db6432906c0f84c2f60437b6dacd450302c3e783d` | current skeleton-only executable surface |
| SCI-002 work package | `fb5cc574fb298ee15a2a935f768a2584bf4d4f0aa84938cf9950e3d17382959a` | blocked correctness workload dependency |
| Minimal 3DoF design | `0e2d0131ea3874ea49cfd4fdbc6f8232009c4cc3231826bf3c4fc45933f1f8e2` | first candidate scientific workload |
| Legacy mission report | `a9f3c3c9066346430b50b607429c39b41074b8ce6e5b443e354536db8d1d0a1d` | behavior/determinism audit only |

Raw hash 只证明审计读取的 bytes；Markdown 中的候选 budget 或本机探测不因出现在已 hash 文档中而获得性能 authority。

### 2.3 当前实现面

[CMake target graph](../../CMakeLists.txt)只有 9 个 interface modules、`gnc_sim --self-check`、skeleton smoke、scientific-convention spike 和治理 validators。不存在：

- MissionSource/Compiler/ExecutionPlan implementation；
- StateLayout、arena、slot、prepared model cache；
- Session lifecycle、StepTransaction、ModelCommit、RunOutcome；
- per-phase timer、allocation counter、high-watermark 或 observation throughput；
- benchmark target、workload manifest、sample runner、report validator；
- performance-qualified build/hardware profile。

当前 MSVC Release build 观察到 `/O2 /Ob2 /DNDEBUG /WX`，但没有显式记录/pin `/fp:*`、FMA contraction、rounding mode、FTZ/DAZ、CPU ISA、LTO/PGO 或 runtime library behavior。因此这只能说明一个本地候选 build 能编译测试，不能支持 D2/D3 声明。

### 2.4 当前 CI 与依赖状态

[CI workflow](../../.github/workflows/ci.yml)配置 Ubuntu 24.04/GCC 13 和 Windows 2025/VS 2026 的 Release correctness jobs，并记录 runner/tool/compiler identity；当前仍没有 benchmark job。[toolchain matrix](../governance/toolchain-support-matrix.json)记录受支持的构建 profile。

`R0-SCI-002` 已完成，[`REF-MINIMAL-3DOF-001`](../../fixtures/ref-minimal-3dof/fixture-manifest.json)已达到 `executable`。后续 benchmark 可以复用该 correctness baseline，并继续拒绝以错误或空计算换取速度。

## 3. Measurement vocabulary 与独立结论

### 3.1 五个正交轴

| Axis | Question | Result shape | 典型失败 |
| --- | --- | --- | --- |
| Correctness | 计算是否符合已批准 scientific/semantic truth | exact facts + field tolerances + convergence | 快但算错、状态/终止错误 |
| Determinism | 在声明的变化边界内，semantic result 是否可重现 | requested/achieved D-level + comparand report | RNG/order/FP/build 漂移 |
| Capacity | 在 size/limit 下是否承载并正确失败 | supported point、high-water、typed limit result | silent drop、unbounded growth |
| Performance | 受控 profile 下资源/时间分布如何 | raw samples + statistics + budget disposition | 单次值、环境不可比、probe effect |
| Real-time qualification | 是否在认证平台满足 deadline/worst-case/阻塞约束 | 独立 qualification artifact | 平均值代替 worst case |

一个 workload 只有 correctness 先通过，performance 样本才有效；determinism fail 不会因 median 更低而通过；capacity limit+1 的 approved rejection 是成功证据，不是性能失败；real-time 资格不从 unpaced benchmark 推导。

### 3.2 测量阶段

每个 workload 将生命周期分成互不混算的 stage：

1. source load/parse；
2. compile/validate/proof；
3. asset prepare/cache lookup；
4. plan image link；
5. Session allocate/bind/initialize；
6. warm-up；
7. steady hot step 或指定 phase；
8. observation encode/enqueue/sink；
9. terminal/finalize/RunOutcome；
10. artifact flush/hash；
11. teardown。

Workload manifest 必须说明 timed stages。除非 case 研究端到端 latency，setup、I/O 与 teardown 不进入 `hot_step`；端到端和 hot-path 同时报告但使用不同 metric id。

## 4. D0–D3 候选可判定语义

### 4.1 共同规则

所有等级都先要求：

- 相同 source/input/asset/plan/seed/command identities；
- correctness gate 通过；
- tick、identity、owner、status、terminal reason、event/order 和 missing/present exact；
- NaN/Inf/domain/fallback disposition exact；
- comparator、normalization 与 excluded metadata 预先声明；
- requested level、achieved level、降级原因和证据 refs 进入 report；
- wall time、CPU time、host path、process id、timestamp、thermal/frequency samples 不进入 physical semantic result。

D-level 描述 model/evidence semantic result，不描述性能样本是否相同。Raw artifact bytes 若包含合法 nonsemantic metadata，可以不同；其 semantic canonicalization 必须 versioned、列出排除字段并同时保留 raw hash。

### 4.2 Level matrix

| Level | 候选变化边界 | Numeric comparand | Exact comparand | 最低证据 | 明确不代表 |
| --- | --- | --- | --- | --- | --- |
| D0 Functional | owner 批准的 build/library/thread/backend 变化 | 逐字段 approved tolerance/invariant/convergence | identity、tick、status、ordering、topology、terminal、RNG case identity | 每个允许 profile 的 correctness/difference report | 同 build exact replay、性能相同 |
| D1 Reproducible | 同 platform/build/binary/FP/thread/input，new process/reset 重放 | 候选要求 canonical numeric payload exact；若 owner 允许 tolerance 必须另立 profile | 全部共同 exact facts、RNG streams、semantic artifact hash | 多次 new-process + reset + order-shuffle replay | cross-compiler、raw timestamp 相同 |
| D2 Cross-build Stable | approved compiler/build pair，各自先满足 D1 | cross-build 逐字段 tolerance/ULP/error budget | structural/discrete/order/status/seed exact | Windows/MSVC 与 Ubuntu/GCC 等批准 pair 的 difference report；禁 fast-math/undefined order | 跨平台 bitwise、固定性能 |
| D3 Bitwise | 指定 OS/CPU ISA/compiler/linker/runtime/libm/fenv/thread/affinity profile | approved semantic numeric payload bitwise exact | semantic payload 全部 exact | 固定 profile 多 process/boot（若要求）binary hashes、FP/thread evidence | 任意机器/OS bitwise 或硬实时 |

表中 D1 numeric exact 与 D3 raw/semantic container 边界是 `PERF-DEC-003` 的候选，不是当前 norm。若 D1 只要求 tolerance，必须提供独立 subprofile/version，不能一份 report 同时把 exact 与 tolerant runs 都称为 D1。

### 4.3 Requested、achieved 与 downgrade

Run/benchmark record 至少包含：

- `requested_level` 与 definition/version；
- `achieved_level`，只能由 validator 从 evidence 推导；
- comparand groups：state、output、observation、diagnostic、event/command、terminal、artifact；
- exact/numeric comparator refs；
- repetitions、process/build/profile matrix；
- first mismatch 的 field/tick/order/artifact；
- downgrade category、cause、owner/waiver ref；
- unavailable/not-executed 与 failed 分离。

Achieved level 是所有 required comparand 和 profile pair 的最低结果。不得因为 final state 一致而忽略中间 trajectory、event order、diagnostic 或 RNG；不得把未执行 D2/D3 写成“通过较低等级”。

### 4.4 Determinism test matrix

| Test id | Variation | 主要判定 |
| --- | --- | --- |
| `DET-RESET` | 同 process reset/replay | state/workspace/RNG/queue 是否完全复位 |
| `DET-PROCESS` | 新 process 重复 | global/static/address/order leakage |
| `DET-CASE-ORDER` | Experiment case 重排/重试 | CaseId-derived RNG 与 artifact identity |
| `DET-THREAD` | approved thread counts/scheduling perturbation | merge/reduction/order 规则 |
| `DET-ALLOC` | allocator/address-space perturbation | pointer/address 不进入 truth |
| `DET-COMPILER` | approved MSVC/GCC build pair | D2 exact + numeric difference |
| `DET-FP` | rounding/FTZ/DAZ/contraction mutation | profile gate 或明确 downgrade |
| `DET-ENCODING` | path/timestamp/JSON property order mutation | raw hash 可变、canonical semantic hash 稳定 |
| `DET-CHECKPOINT` | uninterrupted vs checkpoint/restore | state/RNG/queue/observation continuation |
| `DET-PARALLEL-CASES` | serial vs parallel independent Sessions | per-case semantic result不受调度影响 |

Checkpoint、parallel Sessions 等尚未实现的相应 R3/R6 能力保持 `target_pending`；FP/encoding 等较早可执行项仍要按实际 profile 独立验证，不能由 Legacy 或测试 helper 填补。

## 5. Profile model

### 5.1 Hardware profile

Hardware profile 至少记录：

- stable profile id/version，不包含用户名或明文机器名；
- CPU vendor/model/family/model/stepping/microcode（平台可得时）、ISA flags；
- sockets、NUMA nodes、physical/logical cores、SMT；
- cache topology 与 memory bytes/speed/channel（可得时）；
- GPU/accelerator/driver（workload 使用时）；
- storage/filesystem（I/O workload 使用时）；
- OS edition/version/build/kernel、architecture；
- bare metal/VM/hypervisor/container/WSL 与配额；
- firmware/BIOS profile ref；
- power plan/governor、turbo/boost、frequency policy；
- process affinity、priority、thread count；
- thermal/warm state capture policy；
- monotonic clock source/frequency/resolution/overhead；
- capture tool/version、raw output hash 与敏感字段 redaction policy。

`hardware_profile_hash` 从 approved canonical fields 计算；动态温度、频率、free memory、background load 作为 run environment sample，不改变 profile identity，但可让 run `invalid_environment`。

### 5.2 Build profile

Build profile 至少记录：

- source commit、tree dirty state 与 relevant source hashes；
- CMake/version/generator/configuration/cache；
- compiler/standard library/linker/CRT family 与 exact version；
- 完整 compile/link flags 和 definitions；
- optimization、LTO/PGO、debug/sanitizer/instrumentation；
- FP model、contraction、rounding、exceptions、FTZ/DAZ、libm；
- target CPU/ISA flags；
- third-party dependency/package/asset versions 与 hashes；
- executable/library bytes、size 与 SHA-256；
- build provenance/command/environment refs。

同一个 display label（如 `Release`）不等于同一个 build profile。任何 flag、compiler patch、dependency 或 binary hash 漂移都产生新 profile。

### 5.3 Run environment profile

每次 process 记录：

- hardware/build/workload/measurement profile refs；
- boot/session id 的隐私安全 token；
- start/end UTC 与 monotonic bounds；
- affinity、priority、thread/env vars、working directory policy；
- power plan/governor、hypervisor/container constraints；
- available/used memory、CPU frequency/temperature/background-load samples（可得时）；
- timer calibration；
- process exit、signal/exception；
- input/output/raw-log hashes。

这些字段属于 evidence，不得被模型 kernel读取或改变 simulation state。

### 5.4 本机 audit-only seed

2026-08-10 的只读探测得到：

| Field | Observed value | Baseline disposition |
| --- | --- | --- |
| CPU | AMD Ryzen 9 9950X，16 physical / 32 logical cores，x64 | `audit_only` |
| RAM | 66,193,264,640 bytes，约 61.647 GiB | `audit_only` |
| OS | Windows 11 Pro x64，version/build `10.0.26200` | `audit_only` |
| Virtualization | `HypervisorPresent = true` | incomplete environment，不能忽略 |
| Power | Windows High performance plan | run 时仍需 capture；未授权修改 |
| Timer | high-resolution stopwatch，reported frequency 10,000,000 Hz | 只说明 nominal tick；overhead/precision 未校准 |
| Compiler tools | MSVC 19.50.35725、CMake 4.1.2-msvc8、Ninja 1.12.1、Python 3.13.5 | candidate toolchain only |

本次只读探测使用 `Get-CimInstance Win32_Processor`、`Win32_OperatingSystem`、`Win32_ComputerSystem`、`powercfg /getactivescheme` 和 `.NET Diagnostics.Stopwatch` frequency/high-resolution flags。探测输出未被包装成 baseline artifact；表中只保留与设计审计有关的非敏感字段。

缺少 CPU stepping/microcode/cache/NUMA、firmware、memory speed、affinity、thermal/frequency、background-load、clock overhead 和 binary profile，因此这不是完整 hardware profile，也不产生 baseline id。不得提交 hostname、serial、user path 或其他无关敏感信息。

## 6. Workload manifest

### 6.1 Identity 与 correctness

每个 workload 至少包含：

- `workload_id/version/maturity`；
- owner、purpose、target stage/capability；
- source/fixture/case/input/asset/plan refs 与 full hashes；
- size vector 与 concrete workload point；
- setup/timed/untimed/teardown stages；
- requested determinism level；
- correctness oracle、expected semantic output/digest 与 comparator；
- command/RNG/thread/observation policy；
- metrics/counters、measurement profile 与 budget refs；
- required capabilities 和 `available/target_pending/unsupported`；
- license/provenance；
- mutation children。

Benchmark must consume/validate semantic output outside the timed body so optimizer cannot delete the computation. Digest mechanism itself 必须 versioned，并证明不会改变 timed algorithm；“写入 volatile”或自制 compiler barrier 不能未经验证自动接受。

### 6.2 Size vector

R2/R3 layout workload 的 concrete point 必须绑定所有相关 cardinality，不只写 `small/large`：

```text
entities
model_occurrences
runtime_cells
ports / binding_edges
state_blocks / scalar_state_dimension / state_bytes
held_output_fields / output_bytes
obligations_per_base_tick / rate_classes
continuous_groups / derivative_evaluations_per_step
prepared_assets_count / prepared_asset_bytes
observation_fields / samples_per_tick / encoded_bytes_per_tick
event_entries / command_entries / queue_capacity
sessions / cases / worker_threads
base_ticks / simulated_duration
```

逻辑标签候选为 `tiny`、`nominal`、`stress`、`limit`、`limit_plus_one`，但每个 label 必须解析为 immutable concrete counts。`nominal` 由真实 consumer/scenario 决定；不能由当前硬件恰好跑得快来定义。

## 7. Workload catalog

### 7.1 Calibration 与 R0 seed

| Workload id | Availability | Purpose | 不可作为 |
| --- | --- | --- | --- |
| `PERF-CAL-CLOCK-001` | 激活后可实现 | clock call/loop/harness overhead 与 sample-duration calibration | product latency baseline |
| `PERF-CAL-PROCESS-001` | 激活后可实现 | process launch、environment capture、artifact overhead | hot-step baseline |
| `PERF-R0-M3DOF-001` | blocked by SCI-002 | correctness-gated minimal 3DoF formula/RK4 episode | YYZ/Session capacity |
| `PERF-R0-M3DOF-BATCH-001` | blocked by SCI-002 | 固定 CaseId 集合的多个独立 episode，验证 runner/raw sample/digest | future multi-Session claim |

Calibration 会报告 overhead，不盲目从每个样本相减。若 subtraction 被批准，raw measured、calibration、adjusted 三者都保留，且 adjusted uncertainty 不小于 calibration uncertainty。

### 7.2 R2 Compiler/Plan workload

| Workload family | Size axes | Metrics | Correctness/capacity gate |
| --- | --- | --- | --- |
| `PERF-R2-COMPILE-GRAPH` | entities/cells/ports/edges/rates | parse/compile/proof wall+CPU、peak memory | exact diagnostic/proof/plan hash |
| `PERF-R2-PREPARE-ASSET` | assets/bytes/grid points/cache state | prepare latency、cache hit/miss、prepared bytes | prepared semantic hash/domain facts |
| `PERF-R2-LINK-IMAGE` | descriptors/handles/callsites/state fields | link latency、image bytes、allocations | stable call table/layout proof |
| `PERF-R2-NEGATIVE` | limit/limit+1、invalid graph/rate/owner | rejection latency/memory | stable diagnostic，无 partial plan |

Workload 必须区分 cold file/system cache、warm prepared cache 和 immutable cache reuse；不得只挑最快 cache 状态。Compiler benchmark 不跳过 schema/proof 来制造目标速度，除非 workload 明确研究某个已编译子阶段。

### 7.3 R3 Session workload

| Workload family | Size axes | Metrics | Correctness/capacity gate |
| --- | --- | --- | --- |
| `PERF-R3-INIT` | cells/state/workspaces/prepared refs | allocate/bind/init latency、bytes/allocs | initial committed hash/outcome |
| `PERF-R3-STEP-M3DOF` | ticks/state/obligations/solver evals | step/phase latency、steps/s、sim/wall ratio | SCI-002 trajectory/terminal |
| `PERF-R3-STEP-YYZ` | YYZ cells/rates/closure/observations | phase p50/p95/p99/max、evals、memory | YYZ bundle，formula gates先行 |
| `PERF-R3-OBSERVATION` | fields/rate/encoded bytes/sink | encode/enqueue/flush throughput、drops | exact sample/lineage/backpressure |
| `PERF-R3-QUEUE` | command/event count/capacity | admission/maintenance latency、high-water | exact limit/expiry/rejection |
| `PERF-R3-ROLLBACK` | injected failure position/state size | abort/finalize latency、temporary bytes | committed state unchanged |
| `PERF-R3-FINALIZE` | observation/artifact/outcome size | seal/finalize/flush stages | complete RunOutcome/artifact index |

SoftRealTime、multi-Session/vector、GPU/SIMD 和 Python workloads 在对应能力实现前保持 future catalog entry；不会用串行 skeleton 推导其 budget。

### 7.4 Scale selection

每个 family 至少需要：

- `tiny`：验证 harness/correctness，不作为 capacity conclusion；
- `nominal`：由已批准代表 scenario/consumer 给出；
- `stress`：大于 nominal、仍在 intended supported domain；
- `limit`：声明 capacity 内的最大 point；
- `limit_plus_one`：必须 typed reject/degrade，不 silent drop/clamp；
- 一个单轴 sweep，用于检查复杂度趋势；
- 一个组合 worst-reasonable point，用于暴露 memory/queue/layout interaction。

具体 counts 是 `PERF-DEC-002/008` 的 output。本设计不使用任意 powers-of-two 作为产品承诺。

## 8. Metrics 与 measurement protocol

### 8.1 Required metrics

| Group | Metrics |
| --- | --- |
| Time | wall/CPU duration by stage；per-step/phase raw samples；p50/p90/p95/p99/max；simulated/wall ratio |
| Throughput | plans/s、steps/s、cases/s、observations/s、encoded bytes/s |
| Memory | plan/prepared/state/workspace/queue bytes；peak working/private set；RSS/commit（平台注明） |
| Allocation | hot-path allocation count/bytes、peak outstanding、workspace reuse |
| Capacity | declared capacity、actual high-water、drops/rejections/retries、limit disposition |
| Numerical | solver/derivative/closure evaluations、rejects、iterations；不得与 wall time混为一分 |
| Determinism | requested/achieved、exact hashes、numeric differences、first mismatch/downgrade |
| Instrumentation | timer/counter overhead、tracing enabled/disabled ratio、probe effect |

Metric 必须有 stable id、unit、stage、aggregation、clock/counter source、scope（process/thread/session/phase）、availability/status。平台不支持的 counter 标 `unavailable`，不能填 0。

### 8.2 Run protocol

Measurement profile 至少预先声明：

1. Release build 与 exact binary hash；Debug 只做 correctness，不进入 baseline；
2. clean/dirty tree policy；
3. fresh process 或 in-process sample grouping；
4. affinity、priority、worker threads、power/thermal policy；
5. warm-up count/stop rule；
6. minimum repetitions、minimum sample duration、maximum run time 和 precision/CI stop rule；
7. baseline/candidate interleaving/randomization seed；
8. clock/counter calibration；
9. timed stage boundary 与 I/O/cache state；
10. raw-sample retention；
11. predeclared outlier classification，不删除原值；
12. statistics/interval/bootstrap method 与 tool version；
13. invalid-environment criteria；
14. cancel/failure/retry policy。

Warm-up 样本仍保留并标记 `warmup`，不进入主要 aggregate。Process launch、first-touch、cache-cold 若是研究目标，使用独立 metric/workload；不得从 hot path 隐式丢弃后又在端到端结论中忽略。

### 8.3 Statistics

Report 同时给出 sample count、min/max、median、MAD、指定 percentiles、mean/stddev（若适用）、confidence interval 和每个 raw sample ref。阈值、outlier rule、minimum effect 和 absolute floor 在运行前由 budget version 固定。

回归判定候选形式：

```text
comparable_profile
AND correctness_and_determinism_pass
AND worsening_absolute_delta > absolute_floor
AND worsening_ratio > approved_ratio_limit
AND approved_statistical_rule_triggers
```

统计显著不等于工程重要；只看百分比会放大极短 workload 的 timer noise；只看绝对值会漏掉规模归一化退化。因此 ratio 和 absolute floor 均需 owner 批准。不能多次重跑直到某次通过而丢弃失败 run。

### 8.4 Comparability

Performance comparison 只有在以下 identity 等价或获批 mapping 存在时有效：

- workload/version/input/size vector；
- hardware profile；
- OS/hypervisor/power/affinity/thread policy；
- build profile 除待比较的 change 外；
- measurement profile、clock/counter、stage；
- cache/I/O mode；
- correctness/determinism status。

不等价样本可以作为趋势或 portability data 展示，但 result 为 `not_comparable`，不触发 pass/fail regression。

## 9. Capacity 与资源失败

### 9.1 Plan-time capacity

Compiler/Plan 对 state bytes、workspace、observation rate、queue capacity、prepared asset memory 和 expected cost 产生估算/proof。每个估算记录 formula、inputs、alignment/overhead、unknowns、upper-bound quality 与 actual comparison。Unknown 不得当 0。

### 9.2 Runtime capacity

Session 记录：

- reserved/committed/used/high-water bytes；
- allocation count/bytes by stage；
- state/output/workspace/queue/observation capacity and high-water；
- enqueue/dequeue/drop/reject/backpressure counts；
- solver evaluations/iterations/retries；
- metric collection drop/overrun；
- final resource disposition in RunOutcome。

资源 metrics 是 Artifact/RunOutcome evidence，不进入 physical state_epoch 或模型方程。

### 9.3 Limit semantics

每个 capacity limit 要有：

- owner、unit、scope、profile、workload；
- compile-time reject、initialize reject、runtime backpressure/degrade/terminate 中获批的一种；
- stable status/diagnostic；
- state/queue/artifact atomicity；
- limit 与 limit+1 cases；
- recovery/reset behavior；
- evidence validity effect。

禁止无限 vector growth、silent drop、silent clamp、部分 plan/commit 或靠操作系统 OOM 作为正常边界。

## 10. Budget model

### 10.1 Budget classes

| Class | Meaning | 是否可 gate |
| --- | --- | --- |
| `observation_only` | 建立分布和趋势，不设 pass/fail | 否 |
| `candidate_guardrail` | 在批准 comparable profile 上发现明显 regression | 仅开发 guard，不构成支持承诺 |
| `qualification` | 对明确 consumer/release/profile 的正式 budget | 是，需要 owner/gate evidence |
| `realtime_qualification` | deadline/worst-case/平台资格 | 独立后期 gate，不由普通 benchmark 升级 |

D-009 默认使首轮 baseline 为 `observation_only`。至少有稳定 workload、多个有效 baseline、instrumentation overhead 和 consumer requirement 后，owner 才能提出 guardrail；不能把首个观测值乘一个随意系数就称 budget。

### 10.2 Budget record

每条 budget 至少包含：

- budget id/version/class/owner/approval；
- workload point、hardware/build/measurement profile；
- metric/stage/unit/direction；
- threshold、absolute floor、ratio、percentile/stat rule；
- sample/CI requirements；
- correctness/determinism/capacity prerequisites；
- rationale/consumer/deadline；
- effective/supersession interval；
- waiver/expiry；
- observed baseline refs。

聚合 “performance score” 只能用于 display；任何单项 correctness、capacity、determinism 或 qualification fail 不能被其他更快 metric 抵消。

## 11. Instrumentation boundary

### 11.1 Hot path rules

- timer/counter handles 在 prepare/init 阶段绑定；
- hot step 不做字符串查找、文件 I/O、动态 package 解析或 schema validation；
- per-phase samples 写预分配 buffer 或采样 counter；
- metrics 不改变 model scheduling、RNG、branch、state、output 或 diagnostic order；
- performance summary 在 finalize 后形成 Artifact/RunOutcome resource section；
- trace/sampling level 属于 measurement profile；
- observation sink benchmark 与 kernel-only benchmark 分开。

### 11.2 Probe effect

至少执行 instrumentation-off、counter-only、phase-timing、full-trace profile，比较 correctness hash、allocation behavior 与 performance distribution。若 off/on semantic output 不同，measurement invalid；若 overhead 超出 approved level，报告而不隐瞒。

Allocation profiler、ETW/perf counter、sanitizer、debugger 或 sampling profiler 都会形成独立 instrumented build/run profile，不能与 uninstrumented baseline 混合。

### 11.3 Optimizer guard

Timed computation 的输入来自不可在 compile time 完全折叠的 immutable artifact；输出产生 semantic digest，并在 timed region 后与 expected 比较。Guard 需验证：

- 更改一个 output 会让 correctness fail；
- empty/no-op mutation 的时间与真实 workload 明显可区分但不作为唯一证明；
- generated assembly/IR 或 counter（owner 选择）证明核心 call 未删除；
- digest/validation 不计入 hot metric，另有自身 metric。

## 12. Artifact topology 与 report

激活后的候选逻辑布局如下；本准备切片不创建空 schema/artifact：

```text
benchmarks/
  workloads/
  runner/
  validators/
  mutations/
performance/
  profiles/
    hardware/
    build/
    measurement/
    determinism/
  budgets/
  baselines/
    <baseline-id>/
      manifest.json
      environment.json
      raw-samples.jsonl
      correctness-report.json
      determinism-report.json
      capacity-report.json
      performance-report.json
      instrumentation-report.json
      artifact-index.json
```

实际目录和公共 schema route 由 `PERF-DEC-010/012` 与 ADR 决定。Artifact index entry 至少记录 path/media/schema/bytes/raw SHA-256/canonical SHA-256、producer/build/input refs、authority、validity 和 retention。

### 12.1 Benchmark manifest

Manifest 汇总：

- benchmark/baseline identity、task/gate/commit；
- workload points；
- hardware/build/run/measurement/determinism profiles；
- budget versions；
- commands、producer/validator versions；
- artifacts 与 hashes；
- correctness/determinism/capacity/performance status；
- invalid/waived/pending items；
- owner/reviewer approvals。

### 12.2 Raw samples

每条 sample 至少有 run/process/sample id、workload/profile refs、warmup flag、stage/metric/unit、raw value、clock bounds、iteration count、environment validity、output digest、exit/status 和 anomaly/outlier classification。Aggregate report 只能引用 raw samples，不能覆盖或丢弃它们。

### 12.3 Baseline report

Report 分开展示：

1. completeness/provenance；
2. correctness；
3. requested/achieved determinism；
4. capacity/limit behavior；
5. performance distributions；
6. instrumentation overhead；
7. comparable baseline/candidate difference；
8. budgets/waivers；
9. invalid/unavailable/target_pending；
10. environment and artifact integrity。

Summary 不能只给绿色总分。任何 invalid sample、missing raw artifact、profile mismatch、correctness fail 或 unexplained determinism difference 必须单独计数。

## 13. CI 与运行路由

### 13.1 Pull-request correctness lane

固定 OS image family 的 hosted CI 适合：

- build/benchmark harness smoke；
- tiny workload correctness；
- schema/manifest/mutation validators；
- same-job short determinism repeat；
- artifact completeness。

Hosted hardware、neighbor load 和 image patch 会漂移，因此普通 GitHub-hosted job 的 wall metrics 默认 `informational/not_comparable`，不作为稳定 regression gate。

### 13.2 Dedicated measurement lane

Performance guardrail/qualification 使用 owner 批准的 dedicated/self-hosted/lab profile：

- 固定/记录硬件、OS、firmware、power、affinity；
- 限制 concurrent workloads；
- 环境健康/thermal check；
- immutable build/binary artifact；
- raw sample/artifact retention；
- rerun policy 与审计日志；
- baseline promotion 需 reviewer approval。

### 13.3 Cross-build determinism lane

D2 需要同一 commit/input 的 Windows/MSVC 与 Ubuntu/GCC artifacts，由外部 comparator 比较。每个 build 先分别证明 D1；cross-build comparator 不在其中一个 producer 内读取另一侧 expected。Toolchain profiles 未接受或 hosted evidence 缺失时 result 为 `pending_profile`，不是 D2 pass。

## 14. Frozen Legacy evidence 的正确用途

[Legacy mission report](../../reference/legacy/reproduction/r0-leg-001-20260810-07/mission-report.json)在固定旧工具链中对五个 mission 各运行两次，normalized hashes 一致并标 `deterministic: true`。Normalization 明确排除 generated、wall clock、real-time ratio 与 Output lines/path metadata。这可以证明旧行为 capture 在该语义选择下可重复，但不能证明：

- target RunProfile 的 D1；
- cross-build D2 或 D3；
- raw artifact bitwise；
- current/new product performance；
- 受控统计或容量。

计时本身显示 YYZ `0.168 s` vs `0.040 s`，CAVH `1.799 s` vs `1.773 s`，其他短任务也受进程/缓存/clock noise 影响。它们没有完整 performance profile，因此只作为“为何需要协议”的 audit fact。未来可以用 Legacy workload shape 辅助选择 target scenario，但不能把 Legacy node count、CSV encoding、旧 timer 或 elapsed threshold 迁入 target budget。

## 15. Mutation matrix

同一 production validator 必须执行正常 artifact 和 mutation artifact：

| Mutation id | 注入 | 最早失败 stage |
| --- | --- | --- |
| `PERF-MUT-001` | 不存在/漂移的 architecture section 仍标 resolved | authority/integrity |
| `PERF-MUT-002` | workload input、size vector 或 source hash 缺失 | workload completeness |
| `PERF-MUT-003` | Debug/unknown flags/different binary 复用 Release profile | build identity |
| `PERF-MUT-004` | hardware/OS/power/hypervisor/affinity required field 缺失 | environment completeness |
| `PERF-MUT-005` | 不同 profile 样本直接算 regression pass/fail | comparability |
| `PERF-MUT-006` | hosted image/hardware drift 仍复用 performance baseline | runner identity |
| `PERF-MUT-007` | non-monotonic/low-resolution clock 或 calibration 缺失 | instrumentation |
| `PERF-MUT-008` | warm-up/setup/I/O/flush 混进 hot-step metric | stage boundary |
| `PERF-MUT-009` | 只有 aggregate，无 raw samples/run refs | artifact completeness |
| `PERF-MUT-010` | post-hoc outlier deletion、best-of-N 或 threshold tuning | statistics provenance |
| `PERF-MUT-011` | output unused/no-op core 被 optimizer 删除 | workload correctness |
| `PERF-MUT-012` | wall time/path/PID/timestamp 进入 physical semantic hash | determinism boundary |
| `PERF-MUT-013` | RNG 由 case execution order 派生 | determinism/RNG |
| `PERF-MUT-014` | tolerance 用于 identity/tick/status/order/hash | comparator policy |
| `PERF-MUT-015` | fast-math、unknown fenv 或 nondeterministic reduction 声称 D2 | determinism profile |
| `PERF-MUT-016` | 未固定 platform/libm/thread/fenv 声称 D3 | D3 evidence |
| `PERF-MUT-017` | limit+1 silent drop/clamp/grow 或 partial commit | capacity semantics |
| `PERF-MUT-018` | instrumentation 改变 semantic output/alloc path 且未报告 | probe effect |
| `PERF-MUT-019` | skeleton/CTest/Legacy elapsed 作为 R2/R3 budget | workload authority |
| `PERF-MUT-020` | correctness/determinism/capacity fail 被 aggregate speed score 掩盖 | stage order/summary |

每个 rejection 返回 stable category、stage、subject/workload/profile/sample refs、expected/actual disposition 和 validator hash。Mutation 不得绕过 production parser/comparator 直接测试一个简化 helper。

## 16. 实施切片

### Slice 0：authority 与依赖关闭

- `R0-GOV-001`、`R0-SCI-002` done；
- assignee/reviewer 指派；
- `06 §25` ref 纠正；
- `PERF-DEC-001`–`PERF-DEC-012` 关闭；
- schema/contract/dependency route 经 ADR。

### Slice 1：profile、manifest 与 validator

- hardware/build/run/measurement/determinism/workload/budget schemas；
- raw/canonical hash 与 redaction；
- valid/invalid examples；
- 20 项 production mutation；
- 不产生产品 performance claim。

### Slice 2：calibration 与 minimal 3DoF seed

- clock/process/instrumentation calibration；
- SCI-002 correctness-gated workload；
- output digest/optimizer guard；
- raw samples、statistics、replay；
- 首轮只 `observation_only`。

### Slice 3：R2 workloads

- compile/proof/prepare/link/negative families；
- concrete tiny/nominal/stress/limit/limit+1；
- plan/memory/allocation/correctness artifacts；
- 用 measurement 约束 StateLayout/Plan image decision，不反向改 semantic plan。

### Slice 4：R3 workloads

- init/step/observation/queue/rollback/finalize；
- per-phase time、allocation、memory、high-water；
- D1 replay；
- formula/science/transaction gate 先于 speed。

### Slice 5：baseline、cross-build 与 budget promotion

- dedicated hardware baseline；
- Windows/MSVC + Ubuntu/GCC D2 matrix；
- instrumentation overhead；
- baseline/candidate comparable report；
- owner 批准 observation → guardrail/qualification；
- realtime 始终独立 gate。

每个 slice 单独自审、验证并提交 Git；上一个 slice 的 correctness、artifact 与 owner gate 未关闭时不进入下一个 slice。

## 17. 完整退出检查

- invalid `06 §25` ref 已由 owner 纠正，source/version/hash 可追溯；
- D0–D3 每级 variation/comparand/exact/tolerance/evidence/downgrade 可机器判定；
- wall/performance metadata 与 physical/evidence semantic determinism 边界清晰；
- hardware/build/run/measurement profiles 完整且无无关敏感 host identity；
- workload concrete size vector、source/input/plan/seed/command/output digest/hash 完整；
- SCI-002 correctness 先通过，optimizer guard 能拒绝 no-op/dead-code mutation；
- R2/R3 workload 覆盖 layout/arena/queue 所需 axes 与 limit+1；
- timed stages、warm-up、cache/I/O、clock overhead、raw samples、statistics 预声明；
- performance comparison 只在 comparable profiles 内，hosted drift 标 informational；
- capacity failure typed、atomic、可恢复，无 silent loss/unbounded growth；
- instrumentation off/on correctness 相同且 overhead 可见；
- baseline 首先 observation-only，budget promotion 有 consumer/rationale/owner/version；
- D1/D2/D3 evidence 不由 profile 名或单次 run 推断；
- 20 项 mutation 在最早 stage 被 production validator 拒绝；
- artifact index、raw/canonical hashes、commands/environment/binaries/replay 完整；
- correctness Debug/Release、approved deterministic matrix、dedicated performance run、repository verification 与 `git diff --check` 通过；
- Runtime Numerics Lead、Scientific Authority、Validation Lead、Compiler Lead、Architecture/Evidence owner 具名审查。

## 18. 准备设计自审

- Authority 审查：识别 `06 §25` 不存在并保持 unresolved，没有静默改 backlink。
- 现状审查：current target 只有 skeleton，没有把 CLI/CTest/validator 秒数伪装为 simulation performance。
- 科学审查：correctness gate 先于 timing；每个 workload 绑定 oracle/output digest。
- 确定性审查：D0–D3 候选逐级定义 comparand 与变化边界；wall time 从 semantic truth 排除。
- 数值审查：当前 Release 未显式 pin FP profile，因此 D2/D3 保持未达成。
- 环境审查：本机事实仅 audit seed；hypervisor、timer calibration、thermal/affinity 等缺口明确阻断 baseline。
- 统计审查：raw samples、predeclared warm-up/outlier/stop/threshold、absolute floor + ratio 均为必需，禁止 best-of-N。
- 容量审查：limit 与 limit+1、high-water、typed failure、atomicity 与 recovery 均进入 workload。
- CI 审查：hosted correctness 与 dedicated performance lane 分离；没有用滚动硬件做稳定 gate。
- Legacy 审查：normalized output determinism 与 noisy elapsed seconds 分离，旧结构/计时不进入 target budget。
- 架构审查：准备阶段不新增公共 schema/target，不让 instrumentation 进入模型权威状态。
- 状态审查：backlog、依赖、ADR、CI、CMake、产品和 Legacy 均未修改；本文不是 owner approval、baseline report 或任务完成声明。
