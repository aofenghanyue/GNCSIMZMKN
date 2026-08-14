# ADR-0011: R0 observation-only performance and determinism seed

- Status: Accepted
- Date: 2026-08-15
- Owner: Repository owner
- Related tasks: R0-PERF-001
- Related decisions: D-009

## Context

R0 已有独立 minimal 3DoF 解析 reference 与 C++17 RK4 probe，可以提供正确性先行的 executable workload。目标 Compiler、ExecutionPlan、Session、state layout、arena、queue 和 observation pipeline 尚未实现，因此当前测量无法代表 R2/R3 产品容量或性能。

现有性能准备设计要求多套公共 schema、二十项 mutation、跨构建 artifact 汇聚和 dedicated hardware。当前没有这些 consumer。仓库所有者已要求优先交付可执行结果，并允许当前 R0 范围内的后续推荐项按默认接受记录。

## Decision

1. R0 建立一个独立 `PERF-R0-M3DOF-BATCH-001` workload。它复用 `REF-MINIMAL-3DOF-001` 的线性阻力案例与解析 reference，代码保持在 `benchmarks/r0/`，不链接产品模块或 Legacy。
2. workload 固定 `dt=0.05 s`、每 episode 80 步、经典 RK4 和单线程顺序执行。四个规模点为 1、64、1024、16384 episodes；它们只表达已测 concrete points，不定义产品 `nominal`、stress 或 capacity limit。
3. 计时区域只包含 episode batch 的 RK4 计算。进程启动、参数解析、JSON 写入和独立语义验证分别记录或排除。六个末态分量都进入 timed region 之后验证的 weighted observable，`skip-integrator` 反例必须失败。
4. 本 workload 的 D0 使用独立解析解与显式数值容差。D1 限定为同一 binary、相同输入、相同单线程策略下至少三个 fresh process 的 parsed semantic result 精确一致。该定义只约束当前 R0 workload，不提前固定未来产品 RunProfile 的通用 D1 contract。
5. D2 保持 `target_pending`，直到 approved MSVC/GCC artifacts 由外部 comparator 汇聚。D3 保持 `target_pending`，直到 OS、CPU ISA、compiler/linker/runtime/libm、floating environment、thread/affinity 与所需 boot 边界固定。
6. 首份 Windows/Intel 本机 profile 与 raw samples 的用途为 `observation_only_with_caveats`。计时没有 pass/fail threshold，也不产生 performance regression、SoftRealTime 或 hard realtime 资格。
   当前本地 binary 由 Windows MinGW/GCC 15.1 构建，位于已登记产品支持 profile 之外；baseline 明确记录 `unqualified_local_windows_mingw_observation`。
7. CTest 运行当前 binary 的短 D0/D1 重放、stored baseline 结构校验和五个关键反例。CTest 不比较 wall-time threshold；hosted runner 的计时不进入稳定 gate。
8. fixture-local manifest、profile 与 report 由当前 Python validator 直接消费，不进入产品 runtime，也不建立公共性能 schema。R2/R3 consumer 出现后分别增加其真实 workload、capacity failure 和测量边界。

## Consequences

- R0 获得可复跑的 batch workload、raw samples、环境/build profile、统计分布和 D1 证据。
- 当前 baseline 可以验证 runner、correctness gate、optimizer mutation、profile 隐私边界与统计重算。
- D-007 的 layout/arena 选择仍等待 R2/R3 真实 workload；D-009 继续维持先测量、无硬实时承诺。
- 更换 workload 语义、D1 comparand、计时边界或将 observation 升级为 guardrail/qualification 时，需要新的 owner 决定。
