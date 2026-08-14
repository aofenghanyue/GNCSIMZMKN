# R0-PERF-001：容量、确定性与性能基线

- 状态：executable observation baseline
- 日期：2026-08-15
- 决策：[`ADR-0011`](../adr/0011-r0-observation-only-performance-baseline.md)
- workload：[`PERF-R0-M3DOF-BATCH-001`](../../benchmarks/r0/minimal-3dof/workload-manifest.json)
- baseline：[`R0-PERF-M3DOF-WIN-INTEL12700K-OBS-001`](../../benchmarks/r0/minimal-3dof/baseline-windows-intel-12700k.json)

## 1. 当前可运行结果

R0 使用已完成的 [`REF-MINIMAL-3DOF-001`](../../fixtures/ref-minimal-3dof/fixture-manifest.json) 建立独立 batch benchmark。C++17 executable 在 timed region 内执行 fixture-local 线性阻力 RK4 episode；Python runner 在 timed region 外重算 80 位 Decimal 解析解、校验全部 semantic result、运行 fresh-process D1 比较并汇总 raw timing samples。

该 workload 保持在 `benchmarks/r0/`，不链接 framework 产品模块或 `reference/legacy/`。它测量 standalone numerical workload，可用于验证 benchmark harness、规模轴和确定性协议。Session、Compiler、Plan、state layout、arena、queue、observation pipeline 和 realtime 能力仍无 executable consumer。

## 2. 权威输入

- [03 数学与数值基础 §20](../../design-notes/gnczmkn-architecture-roadmap/03-mathematics-and-numerical-foundation.md#20-数值确定性等级)定义 D0–D3；§22 要求记录平台、编译选项、性能与确定性；§23 要求 benchmark 记录规模、CPU、编译选项和确定性等级。
- [06 Session 内核 §16](../../design-notes/gnczmkn-architecture-roadmap/06-simulation-kernel-time-and-lifecycle.md#16-确定性)定义确定性来源；§18 定义资源预算与 phase metrics。
- backlog 原先引用 `06 §25`，当前 06 分册只到 §22。任务引用已收敛到实际存在的 §16 和 §18。
- [D-009](../handoff/open-decisions.md)维持“先记录测量，不承诺硬实时”。
- workload 的科学输入、解析 final state 与容差来自 minimal 3DoF fixture、oracle 和独立 Python reference。

## 3. Workload 与规模

每个 episode 固定：

- model：`MODEL-MINIMAL-3DOF-LINEAR-TRANSLATION-001`；
- `dt = 0.05 s`，duration `4 s`，80 fixed steps；
- classical RK4，每步四次 derivative evaluation；
- state dimension 6；
- 单线程、固定 episode 顺序；
- 初态按 manifest 中的整数 pattern 产生小幅确定性变化，避免 batch 退化为同一 episode 的常量复制。

| Point | Episodes | RK4 steps | Derivative evaluations | 结论范围 |
| --- | ---: | ---: | ---: | --- |
| `smoke-1` | 1 | 80 | 320 | harness/correctness |
| `batch-64` | 64 | 5,120 | 20,480 | scale observation |
| `batch-1024` | 1,024 | 81,920 | 327,680 | scale observation |
| `batch-16384` | 16,384 | 1,310,720 | 5,242,880 | largest tested observation |

这些 concrete points 只证明当前 standalone harness 已测范围。`nominal`、stress、limit、limit+1 和 R2/R3 capacity 需要对应产品 artifact 与 consumer 后再定义。

## 4. 正确性与 optimizer guard

Timed region 只包含 RK4 batch。进程启动、参数解析、report encode/write 和 Python 解析验证分别记录或排除。六个末态分量都进入 weighted observable；runner 同时检查：

- episode、step、derivative-evaluation 和 weight counts exact；
- mean final position 对独立解析 aggregate 的绝对误差不超过 `2e-8 m`；
- mean final velocity 的绝对误差不超过 `4e-9 m/s`；
- weighted observable 的绝对误差不超过 `3e-7`；
- 所有 numeric output finite；
- `skip-integrator` 产生的更快输出由同一 correctness comparator 拒绝。

Performance sample 只有在上述正确性全部通过后才有效。Wall time、process time、path、timestamp、compiler identity 和 environment metadata 均排除在 semantic result 之外。

## 5. D0–D3 当前状态

| Level | 当前 workload 定义 | 状态 |
| --- | --- | --- |
| D0 | Debug/Release 或支持 build profile 下，identity/discrete facts exact，numeric output 通过解析容差 | executable/pass |
| D1 | 同一 binary、输入与单线程策略，至少三个 fresh process 的 parsed `semantic_result` exact | executable/pass |
| D2 | approved MSVC/GCC pair 各自先通过 D1，再由外部 comparator 检查 exact facts 与 numeric differences | `target_pending` |
| D3 | 固定 OS、CPU ISA、compiler/linker/runtime/libm、floating environment、thread/affinity 与必要 boot 边界 | `target_pending` |

D1 的当前定义只服务这个 R0 workload。未来产品 RunProfile 的通用 D1 comparand 仍需随真实 Session/Artifact consumer 冻结。

## 6. Baseline profile 与测量协议

本地硬件 profile 为 [`R0-LOCAL-WIN11-INTEL12700K-HYPERV-001`](../../benchmarks/r0/minimal-3dof/hardware-profile-windows-intel-12700k.json)：

- Intel Core i7-12700K，12 physical / 20 logical processors；
- 32 GiB installed memory；
- Windows 11 Pro build 26100；
- Hyper-V present；
- High performance power scheme；
- process 使用 Normal priority、全部 20 个可见逻辑处理器、一个 worker thread；
- frequency、temperature、background load、NUMA、microcode 和 memory channel 数据不可用。

当前 Release binary 报告 `gcc-15.1.0`，来源为本地 Windows MinGW toolchain。它超出当前产品支持 profile，baseline 将 build classification 固定为 `unqualified_local_windows_mingw_observation`，且 `product_qualification=false`。

每个 point 保存两个 warm-up process 与九个 measured process，共 44 条 raw samples。统计由 raw samples 重算：min、max、median、MAD、p95、mean 和 population standard deviation。Python `perf_counter_ns` 先校准，calibration 不从 workload samples 中相减。所有 raw values 保留，没有 post-hoc outlier 删除。

Baseline class 为 `observation_only`，`performance_thresholds` 为空。当前计时可以展示规模趋势，也可以作为未来 dedicated baseline 的协议种子；它不触发 CI wall-time regression。

## 7. 可执行入口

生成本机 Release baseline：

```powershell
python -I -B tools/r0_performance_baseline.py `
  --manifest benchmarks/r0/minimal-3dof/workload-manifest.json `
  --cases fixtures/ref-minimal-3dof/cases.json `
  --oracle oracles/ref-minimal-3dof/reference.json `
  --hardware-profile benchmarks/r0/minimal-3dof/hardware-profile-windows-intel-12700k.json `
  --baseline benchmarks/r0/minimal-3dof/baseline-windows-intel-12700k.json `
  --executable build/release/gnc_minimal_3dof_benchmark.exe `
  --configuration release `
  --write-baseline
```

直接验证当前 binary、stored baseline 与反例：

```powershell
ctest --test-dir build/release -R "r0.performance-minimal-3dof" --output-on-failure
```

Repository verification 使用 `--static-only` 重算 source hashes、raw-sample aggregates、D0/D1 状态、capacity scope 和 profile privacy；CTest 额外启动当前 Debug/Release binary 做 fresh-process replay。

## 8. 直接失败用例

同一 production validator 拒绝五类关键回归：

1. `skip-integrator` 输出无法通过解析 comparator；
2. zero-episode input 被 C++ executable 拒绝且不生成 report；
3. workload 在缺少 cross-build artifacts 时声称 D2；
4. aggregate statistic 与 retained raw samples 不一致；
5. hardware profile 写入 hostname 等敏感字段。

## 9. 当前限制与后续激活

- 本结果没有 target Session/Compiler/Plan 性能信息，也没有 state/arena/queue capacity 结论。
- 本地 hardware 与 build profile 带明确 caveat，普通 hosted CI 只运行 correctness、D1 和结构检查。
- D2 需要 approved Windows/MSVC 与 Ubuntu/GCC producer artifacts 和独立 comparator。
- D3、multi-Session、checkpoint、parallel cases、instrumentation overhead 和 realtime qualification 随对应阶段能力激活。
- 首个 R2 layout consumer 出现时复用 manifest 的 concrete size-vector 方式，增加真实 graph/state/slot/asset counts 和 typed limit failure。
- 首个 R3 Session consumer 出现时增加 init/step/observation/queue/rollback/finalize 分段测量；科学与事务正确性继续先于 timing。
