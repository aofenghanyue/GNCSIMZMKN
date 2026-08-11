# REF-MINIMAL-3DOF-001 独立参考设计

- 文档状态：Preparation design
- 实现成熟度：未实现；不得作为 G1、Compiler 或 Session pass evidence
- 任务：`R0-SCI-002`（backlog 保持 `planned`）
- 日期：2026-08-10
- Authority owner role：Model SDK Lead
- Scientific approval role：Scientific Authority
- Numerical approval role：Runtime Numerics Lead

## 1. 目的与非目标

本 reference 用最小的三轴平移动力学建立四类独立事实：解析轨迹、固定步 RK4 收敛、网格终止和数值领域失败不提交。它未来可以被 model kernel、Compiler/Plan fixture 和 Session transaction test 共同消费，但在 R0 只是一组 standalone scientific/evidence artifacts。

不进入本 reference 的内容包括姿态、质量/力闭合、气动、大气、地球旋转、传感器、制导控制、随机数、事件求根、非网格 partial step、CSV 编码和 Legacy 类/节点。它不实现产品 `NumericalOutcome`、`ModelCommit`、`RunOutcome` 或任何 R1–R3 公共类型。

## 2. 当前 seed 与契约缺口

当前 [`fixture-manifest.json`](../../fixtures/ref-minimal-3dof/fixture-manifest.json) 的身份为 `REF-MINIMAL-3DOF-001`，状态 `specification_only`，raw SHA-256 为 `8e90bdefcf8d5250f22b147a061e7a5934c9618d08bc9f63e6407a32732f579a`。它声明需要 mission source、initial state、analytic/independent trajectory、termination、dt convergence 和 tolerance，但目录内没有其他文件。

`gnczmkn.fixture-manifest/1` 能表达 expected facts 与 evidence refs，却没有 case/model payload、逐文件/聚合 input hash、trajectory record、convergence report 或 failure commit fact 的结构。激活前由 R0-SPEC-001 owner 选择已批准 sidecar，或通过 ADR 建立新公共 schema；本设计中的字段和文件名只表达信息需求，不提前冻结公共 wire contract。

R0-SCI-001 的 executable convention profile 当前 SHA-256 为 `82887511e06dd1b36d5e7c45d5073a0b03654f049b475d879b5ff154e0d44194`，但 ADR-0006/0007 仍为 Proposed。因此 SI、frame、binary64、`1e-12` 与 integer-tick profile 在此均是依赖候选，只有 dependency owner 接受后才能成为 normative input。

## 3. Fixture-local 科学模型

### 3.1 State 与 metadata

| Quantity | Fixture-local semantic | Geometry | Expressed/reference frame | Unit |
| --- | --- | --- | --- | --- |
| `r` | body point position relative to fixture origin | point | `frame.fixture.minimal3dof.inertial@1` / same origin | m |
| `v` | body velocity relative to fixture origin | free vector with translational kinematics | `frame.fixture.minimal3dof.inertial@1` / same origin | m/s |
| `a0` | constant applied acceleration | free vector | `frame.fixture.minimal3dof.inertial@1` | m/s² |
| `lambda` | isotropic linear velocity-decay rate | scalar | frame-independent | 1/s |

State 是 `[r_x,r_y,r_z,v_x,v_y,v_z]` 的概念序列；机器记录必须通过 semantic id 访问，不能依赖 JSON member、数组、CSV 或 C++ 内存顺序。所有输入和输出为 finite IEEE-754 binary64，`lambda >= 0`，无单位猜测或隐式 frame transform。

### 3.2 Evolution

对 `tau = t - t0`，逐轴方程为：

```text
dr/dt = v
dv/dt = a0 - lambda * v
```

`lambda = 0` 时闭式解：

```text
v(t) = v0 + a0 * tau
r(t) = r0 + v0 * tau + 0.5 * a0 * tau^2
```

`lambda > 0` 时令 `v_inf = a0/lambda`：

```text
v(t) = v_inf + (v0 - v_inf) * exp(-lambda*tau)
r(t) = r0 + v_inf*tau + (v0 - v_inf) * (1 - exp(-lambda*tau))/lambda
```

实现必须为 `lambda=0` 使用独立解析分支，不能先计算 `a0/lambda` 再靠容差或非有限值恢复。

### 3.3 时间与步进

- `time_origin = 0 s`；
- `t_k = time_origin + integer_tick * dt`；
- duration 使用 ExactGrid，`duration/dt` 必须为非负整数；
- 数值 probe 使用 classical explicit RK4，stage 时间为 `t`、`t+h/2`、`t+h/2`、`t+h`，权重 `1/6,1/3,1/3,1/6`；
- 每个 step 在局部 candidate 上完成全部 stage 与 finite/domain checks 后才更新 reference committed state；
- reference script 不实现自适应步长、dense output、event root 或 final partial step。

## 4. Case inventory

### 4.1 `CASE-MIN3D-CONSTANT-ACCELERATION`

用途：固定初值、SI/frame/time 和逐 tick 闭式轨迹；它不承担收敛阶证明。

```text
t0 = 0 s, dt = 0.5 s, duration = 2 s, ticks = 0..4
r0 = [100, -50, 1000] m
v0 = [20, 5, 10] m/s
a0 = [0.5, -1, -9.80665] m/s^2
lambda = 0 1/s
```

解析终点候选：

```text
r_4 = [141, -42, 1000.3867] m
v_4 = [21, 3, -9.6133] m/s
```

最终 expected 必须由独立 reference 生成并保存全部 5 个 tick，文档数字只是设计复核，不是可执行 evidence。

### 4.2 `CASE-MIN3D-LINEAR-DRAG-CONVERGENCE`

用途：以非多项式闭式解验证固定步 RK4 的 global order。

```text
t0 = 0 s, duration = 4 s
dt ladder = [0.8, 0.4, 0.2, 0.1, 0.05] s
r0 = [0, 0, 100] m
v0 = [10, -5, 20] m/s
a0 = [1, 0, -9.80665] m/s^2
lambda = 0.2 1/s
```

解析终点设计值：

```text
r(4) = [33.7667758970695, -13.7667758970695, 93.9400564392741] m
v(4) = [7.24664482058611, -2.24664482058611, -18.0146112878548] m/s
```

一次非权威设计计算得到下表；激活后必须由受控 Python/C++ 工具重新生成并由 owner 审查：

| dt (s) | position L∞ error (m) | velocity L∞ error (m/s) | adjacent observed order |
| ---: | ---: | ---: | ---: |
| 0.8 | 7.744573947263689e-4 | 1.548914789459843e-4 | n/a |
| 0.4 | 4.527327401149250e-5 | 9.054654796614159e-6 | 4.09645 |
| 0.2 | 2.736684976412107e-6 | 5.473369952824214e-7 | 4.04816 |
| 0.1 | 1.682137167335895e-7 | 3.364275258377347e-8 | 4.02406 |
| 0.05 | 1.042610620061168e-8 | 2.085219108494130e-9 | 4.01202 |

候选 acceptance 为 position/velocity error 各自严格下降，且四个 adjacent order 均不低于 3.8。最终阈值、rounding 与平台 margin 由 Runtime Numerics Lead 批准。

### 4.3 `CASE-MIN3D-EXACT-GRID-TERMINATION`

用途：给出没有 root interpolation 歧义的 analytic termination reference。

```text
t0 = 0 s, dt = 0.5 s, requested duration = 10 s
r0 = [0, 0, 10] m
v0 = [1, 0, -2] m/s
a0 = [0, 0, 0] m/s^2
lambda = 0 1/s
predicate = position.z <= 0, evaluated on committed ticks
```

预期首次满足为 tick 10、`t=5 s`、`r=[5,0,0] m`、`v=[1,0,-2] m/s`，reference trajectory 包含 tick 0..10，不包含 tick 11。observation-before-RunOutcome 的事务顺序由 `ORACLE-YYZ-STOP-06`/R3 验证，不由本 scientific case 冒充。

非网格 crossing、StopBefore/StopAfter、dense root 与 interpolation 留作明确 follow-up；未批准 policy 前不得在本 case 中隐式取整。

### 4.4 `CASE-MIN3D-RK-STAGE-DOMAIN-FAILURE`

用途：确定性验证数值领域失败时 candidate 不成为 reference committed state。该 case 是显式非物理 guard fixture，不参与轨迹科学权威。

```text
t0 = 0 s, dt = 0.5 s, requested duration = 1.5 s
r0 = [0, 0, 0] m
v0 = [1, 0, 0] m/s
a0 = [0, 0, 0] m/s^2
lambda = 0 1/s
evaluation domain = evaluation_time < 0.75 s
```

step 0 的 RK4 stages 位于 0、0.25、0.25、0.5 s，成功提交 tick 1：`r=[0.5,0,0] m`、`v=[1,0,0] m/s`。step 1 的 k2 位于 0.75 s，必须以 fixture-local `reference-domain-error` 失败；candidate 被丢弃，last committed tick/state/hash 保持 tick 1。该 label 不是未来产品 DiagnosticCode 或 RunOutcome contract。

## 5. Comparison 与 tolerance

| Field class | Comparison |
| --- | --- |
| case/fact/model/convention identity | exact |
| tick、stage、status、terminal predicate identity | exact |
| time | 由 tick 计算；tick exact，浮点表示按批准 time tolerance |
| position | 每分量 `abs(diff) <= atol_r + rtol_r*max(abs(ref),abs(candidate))` |
| velocity | 每分量使用独立 `atol_v/rtol_v`，不与 position 合并 |
| finite/domain | NaN/Inf 总是失败；不能由 tolerance 接受 |
| convergence | `p_h = log2(E(h)/E(h/2))`，position/velocity 分开；候选阈值 `p_h >= 3.8` |
| termination/failure commit | tick/state identity exact，state 数值按对应字段 tolerance |

R0-SCI-001 的 `absolute=relative=1e-12` 可作为初始候选，但 ADR/owner 接受前不能写成最终 authority。validator 必须限制 tolerance 的字段范围与批准上限，不能通过提高 tolerance 让 sign、stage、order、NaN 或 off-by-one mutation 通过。

## 6. 独立实现拓扑

```text
approved case/model payload
        |                 |
        v                 v
CPython analytic      C++17 RK4 probe
standard library      standard library
        |                 |
        +--------+--------+
                 v
      semantic comparator + convergence/failure validator
                 |
                 v
       evidence report + artifact index
```

- Python 只实现闭式公式、terminal truth 和 case expected；优先以标准库 `decimal` 的高精度上下文计算解析 truth，再显式投影为带 decimal text 的 expected；不得调用 RK4 probe、项目代码、Legacy、NumPy/SciPy 或外部进程；
- C++17 独立实现 fixture-local RHS、classical RK4、domain/finite check 和 candidate/commit bookkeeping；不得读取 expected，不得 include/link Eigen、产品模块或 Legacy；
- schema-approved case payload 是输入 authority。为避免给 standalone C++ spike 引入 JSON/第三方 parser，四个小 case 可在 C++ test source 中保留独立 literal input table，但程序必须回显每个 semantic input；validator 将回显逐字段与 payload exact 对照，任一副本漂移即失败；
- Python 与 C++ 不读取对方输出作为输入；解析公式与数值算法不共享实现代码；
- comparator 以 semantic id 对齐，不比较 JSON member order、数组 storage order、路径或 CSV 列；
- validator 生成 cross-tool values、逐状态误差、convergence、terminal、failure 与 mutation results；
- planned validation target 是 R0 test spike，不成为产品 math/model API。

具体候选文件包括 fixture-local model/cases/expected payload、Python analytic reference、C++17 probe、PowerShell validator 和 JSON evidence report；名称与 schema 在第 2 节决策关闭后再冻结。

## 7. Provenance 与 evidence

每次受控生成至少记录：

- fixture/model/case/convention/algorithm identity；
- 每个输入逻辑路径、byte count、SHA-256 与确定性聚合 input hash；
- Python/C++/validator/CMake source hash；
- OS、architecture、compiler、CMake、generator、build type、Python、PowerShell；
- exact commands、working directory、exit code、stdout/stderr hash；
- high-precision analytic expected、C++ input echo 与 raw semantic results；
- per-field error/tolerance、position/velocity convergence table；
- terminal first-satisfied tick/state；
- failure stage、candidate disposition、last committed state/hash；
- deterministic rerun comparison；
- mutation rejection inventory；
- 所有 artifact 的 byte count/hash 与零缺失复核。

报告里的时间戳、绝对临时路径和 wall duration 不参与 scientific semantic hash，但仍保留在 raw environment evidence；normalized 边界必须显式且不可随运行扩张。

## 8. Target 消费边界

- R1：model kernel 用同一方程/案例做纯函数与 definition/state/input/output/telemetry/kernel 单测；
- R2：Compiler 把 fixture source 编译为稳定 state/time/integration/observation plan，reference 本身不依赖 plan 内节点数；
- R3：Session 运行 candidate 后按 semantic fields 比较 trajectory、terminal 和 failure rollback；
- R4：JSON/CSV/MAT 只是同一 Observation semantic dataset 的编码，不能反向定义 truth；
- 任一 target comparison 都在 target 独立运行后读取 artifact；target 禁止 include、link 或调用 reference/Legacy executor；
- target 与 reference 差异按科学差异模板分类，unexplained 直接阻断切换。

## 9. Mutation 与防空验证

工作包中的 `SCI3-MUT-001`～`016` 是最低矩阵。正向 validator 与 mutation 必须共享 evaluator，并额外验证：

- 删除任一 case/fact/required artifact 时 completeness 失败；
- duplicate/unknown case、fact、semantic field 或 tolerance id 失败；
- dt ladder 非 ExactGrid、未严格递减或 pair ratio 非 2 时 convergence 输入失败；
- observed order 使用错误对数底、反向 error ratio 或混合单位 error 时失败；
- `lambda<0`、非有限 input、负 tick/dt/duration 或无效 frame/unit 时输入失败；
- failure case 没有 raw failed candidate evidence，仅手写 expected 时 evidence sufficiency 失败；
- zero target files 只能记为 target-pending，不能把 reference self-check 宣称为 Session pass；
- fixture 状态改为 executable 但 evidence refs/hash/owner review 不完整时状态一致性失败。

## 10. 准备退出检查

- 单一最小方程同时有 `lambda=0` 与 `lambda>0` 的可审计闭式解；
- 四个 case 分别负责轨迹、收敛、终止和失败，不以一个 case 伪证所有能力；
- position/velocity 的 semantic、unit、frame、time 与 tolerance 分离；
- convergence 使用非多项式闭式 truth，避免 RK4 的 vacuous exact pass；
- failure 不依赖溢出、NaN serialization 或异常文案，且 last committed state 明确；
- Python analytic/C++ RK4/validator 三层独立性和零产品/Legacy 依赖明确；
- payload/schema/hash 决策保留 R0-SPEC-001 + ADR gate；
- target objects 保持 pending，没有把 validation spike 当产品实现；
- 16 个代表 mutation 和额外 completeness failure 已定义；
- fixture/backlog/schema/产品/Legacy 零修改，任务仍为 `planned`、assignee 为空、依赖仍为 `review`；
- UTF-8、Markdown links、repository verification 与 `git diff --check` 通过。
