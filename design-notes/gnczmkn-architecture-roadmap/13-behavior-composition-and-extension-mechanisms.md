# 13｜行为组合、嵌入机制与共享权威

[上一册：运行对象模型与组件内部构成](12-runtime-object-model-and-component-anatomy.md) · [返回总索引](README.md) · [下一册：周期数据流、状态事务与连续闭合](14-cycle-dataflow-state-transaction-and-continuous-closure.md)

**主线定位**：本册位于 Runtime Cell 内部，吸收算法、状态机、滤波、限幅、锁存、协议和局部求解等高变化行为。它把局部 state fragment 合入唯一 owner 的 Recipe；只有出现共享权威、独立时钟或独立资源边界时才向 Runtime Cell 晋升。

## 本册一口气读完：anti-windup 留在宿主内

`REF-YYZ-001` 的 pitch controller recipe 嵌入一个 anti-windup mechanism。它声明 `AntiWindupDefinition{k_aw=0.35}`、`AntiWindupStateFragment{integrator_correction}` 和纯 `evaluate()`；宿主将 fragment 合入 `state:control`，每次 `SampledUpdate` 把 mechanism result 与控制律 result 合成一份 ComponentDelta。

mechanism 不获得端口、schedule、Session lifecycle 或独立 Diagnostic policy。若导航源选择必须由 guidance、control 和 evaluator 共同观察，则建立专门的 navigation-source StateOwner/DecisionAuthority cell，并通过 typed snapshot 传播。两种落位都沿 Runtime Cell Recipe 展开，Kernel 无 mechanism 类型分支。

## 1. 本册结论

未来最容易膨胀的区域位于 Runtime Cell 内部：算法公式、状态切换、滤波、故障处理、增益调度、约束、交班逻辑和局部数值迭代会持续增加。目标架构通过 Runtime Cell Recipe 与可嵌入 mechanism 吸收这类变化。

三种对象保持不同尺度：

- `AlgorithmKernel` 表达领域计算；
- embedded mechanism 表达可复用的局部行为工具；
- Runtime Cell 表达状态、时间、DecisionAuthority、failure 和 resource 的运行边界。

mechanism 没有 Session identity、独立端口、调度、生命周期或 logger。它的 Definition、StateFragment 和求值函数由宿主 Runtime Cell Recipe 组合，状态最终归宿主 `CommittedStateStore` block 所有。

状态机属于 embedded mechanism。滤波器、滞回器、抗饱和器、debounce、故障锁存、交班协议和局部迭代器也属于同一类扩展工具。共享飞行阶段、导航源或物理构型需要跨组件一致观察时，单独建立窄 StateOwner/DecisionAuthority cell；内部工具仍保持嵌入形态。

## 2. 为什么需要行为组合层

当前组件通常把下列内容揉在一个类中：

- JSON/config 读取；
- 资产装载；
- 算法参数与公式选择；
- 运行状态；
- 状态切换和故障逻辑；
- 端口绑定；
- 调度入口；
- 输出缓存；
- observable getter；
- 日志和错误文本。

只拆分 RuntimeComponent 与 AlgorithmKernel 仍然不足。复杂组件会把全部非公式逻辑重新堆到壳中，最终形成新一代巨型组件。行为组合层承担两个任务：

1. 给局部行为工具提供一致的嵌入规则；
2. 给“何时升级为独立运行边界”提供明确门槛。

它归 model SDK 管理，独立运行子系统不在其职责范围内。

## 3. Runtime Cell Recipe

Package 为每个运行边界提供一个 recipe：

```text
RuntimeCellRecipe {
  definition_schema
  algorithm_definitions[]
  state_fragments[]
  input_contracts[]
  output_contracts[]
  local_composition
  execution_obligations[]
  lifecycle_requirements
  observation_projection
  verification_refs[]
}
```

Recipe 处于 package/compile time。它不会在每步解释执行。model SDK 根据 recipe 生成或验证：

- canonical ModelDefinition；
- 合并后的 StateSchema；
- typed component input/output structures；
- 一个或多个 obligation entry；
- config/schema/documentation metadata；
- mechanism attribution path；
- package reference tests。

ExecutionPlanDescriptor 只保存展开后的 state、port、obligation、callsite 和 implementation identity。Kernel 无需遍历 recipe，也不认识局部 mechanism 类型。

## 4. 内部组合单元

### 4.1 AlgorithmKernel

用于领域公式、估计器更新、控制律、气动计算、动力学方程或论文算法：

```text
evaluate(
  const AlgorithmDefinition&,
  const AlgorithmStateView&,
  const AlgorithmInput&,
  EvaluationContext&
) -> AlgorithmResult
```

Kernel 读取显式输入，返回显式结果。它不解析配置、不查 registry、不记录文件日志、不持有 Session 指针。

### 4.2 Embedded mechanism

用于与具体领域公式相对独立、可在多个组件内复用的行为规则：

```text
MechanismDefinition       immutable configuration
MechanismStateFragment?   owner-managed semantic memory
MechanismInput            local typed input
MechanismResult           next state fragment + local decision/value/diagnostic
```

架构不要求所有 mechanism 继承同一个 C++ 基类。package 可以使用纯函数、显式类、窄模板或生成代码。必须满足的契约只有：

- Definition 在运行中 immutable；
- StateFragment 能被宿主初始化、reset、checkpoint 和验证；
- evaluate 对外部系统无副作用；
- result 只修改自己的 state fragment 或返回局部值；
- 诊断以结构化 draft 返回；
- scratch 放入宿主/invocation workspace，不伪装成语义状态。

### 4.3 Explicit Adapter

用于单位、frame、contract representation 或 sample semantics 的转换。Adapter 无隐藏记忆；若转换需要滤波、hold 或估计状态，应由带 StateFragment 的 mechanism 承担。

### 4.4 Projection 与 invariant

Projection 从宿主 state/result 生成正式 output 或 telemetry。Invariant 在 candidate 提交前验证 owner block。两者都不保存第二份权威状态。

## 5. StateFragment 合成

每个 stateful mechanism 声明一个具名 state fragment：

```text
ControllerState {
  tracking_filter      // FilterState
  mode_logic           // ModeState
  anti_windup          // AntiWindupState
  fault_latch          // FaultLatchState
  algorithm_state      // ControllerAlgorithmState
}
```

model SDK 在编译期完成：

1. fragment name/schema 冲突检查；
2. initialization/reset/checkpoint coverage；
3. semantic state 与 workspace 分离；
4. mechanism result 到完整 owner `StateReplacement` 的组装；
5. observation projector 的 FieldId 映射。

StepTransaction 仍然只看到宿主 owner block 的完整 replacement。mechanism 不能获得独立 state handle，也不能绕过 owner reducer 直接提交。

共享一个 mechanism instance state 的需求通常表示隐藏 StateOwner/DecisionAuthority。此时应明确选择一个宿主 owner，通过 typed snapshot 或 command/event contract 与其他组件交互。

## 6. Local Composition

组件内部通常使用一条静态、显式的 local pipeline：

```text
input adaptation
-> quality/freshness gate
-> local mode or protocol decision
-> domain AlgorithmKernel
-> limiter / anti-windup / fault response
-> output projection
-> owner state replacement
```

该 pipeline 可以手写为 package-specific composition，也可以由窄 recipe builder 生成。它遵守下列规则：

- 顺序和数据依赖在编译时固定；
- 局部值使用 typed stack/workspace data，不进入全局 CycleFrame；
- 同一 state fragment 只有一个局部 writer；
- 局部环需要明确的小型 solver mechanism，不能依赖回调顺序碰巧收敛；
- 任一步失败使整个宿主 obligation 返回 failure，owner delta 不提交；
- 局部 mechanism 不产生跨组件 command/event，宿主决定哪些结果升级为正式 contract。

无需在 Kernel 中建立“组件内部通用图解释器”。蓝图工具可以编辑 recipe metadata，Compiler 最终仍生成静态 composition entry。

## 7. 常见 mechanism 家族

下表用于说明扩展方向，不构成封闭枚举。

| 家族 | 典型工具 | 拥有的语义状态 |
| --- | --- | --- |
| mode/protocol | 小型状态图、交班协议、启动序列 | current state、timer、pending request |
| conditioning | debounce、hysteresis、deadband、rate limiter | previous value、counter、latch |
| filtering | 一阶滤波、互补滤波、outlier gate | filter state、quality memory |
| constraint handling | saturation、anti-windup、projection | integrator correction、active constraint |
| selection/arbitration | gain schedule、source ranking、fallback chain | hysteresis、selected candidate |
| fault behavior | dropout、stuck、bias drift、fault latch | fault mode、seed/cursor、latched flags |
| local numerics | root solve、active set、warm start | 只有会影响未来结果的 warm state |
| temporal helper | timer、sample counter、timeout | simulation tick based counters |

通用工具进入 `framework/model_sdk/behavior` 前需要至少两个真实组件证明相同语义。项目论文中的特殊切换规则、经验限幅和临时故障模型留在 project package。

## 8. 状态机作为 mechanism 示例

最小状态图工具可以由以下内容组成：

```text
ModeGraphDefinition {
  states[]
  transitions[] { transition_id, source, target, guard_id, priority }
  initial_state
  conflict_policy
}

ModeState {
  current_state
  entered_tick
  local_counters
  pending_request_ids
}

evaluateMode(definition, state, typed_inputs, tick)
  -> ModeDecision
```

`ModeDecision` 只描述选中的 transition、原因和候选 next state。宿主 reducer 负责：

- 把 next `ModeState` 合并进 owner replacement；
- 选择本次使用的 AlgorithmDefinition/kernel；
- 对积分器、滤波器或其他 fragment 执行明确 state mapping；
- 产生需要跨组件传播的正式 output/event；
- 生成 telemetry 与 DiagnosticDraft。

guard 是纯判定。计时使用 simulation tick。enter/exit 逻辑不能以任意 callback 修改其他 state fragment；跨 fragment 的映射由宿主 transition reducer 显式完成。

层次状态图、正交 region 和 run-to-completion microstep 只有在真实组件无法通过多个小 mechanism 组合时才进入 SDK 演进。它们不会自动扩充 Session 或 Kernel。

Session lifecycle 与 Workflow task lifecycle 也会使用显式状态迁移模型。它们分别归 Session owner 和 Workflow owner 管理，与 GNC 组件的 mode mechanism 没有共享运行身份。实现可以复用纯 transition helper，系统不建立全局状态机服务。

## 9. 其他组合示例

### 9.1 制导组件

```text
NavigationEstimate
-> validity gate mechanism
-> guidance phase mechanism
-> selected GuidanceAlgorithmKernel
-> command constraint mechanism
-> GuidanceReference
```

阶段图与 counters 位于 GuidanceState。新增论文制导律通常只增加 AlgorithmDefinition/Kernel；阶段工具可以复用，也可以由项目定义更窄的 mechanism。

### 9.2 姿态控制组件

```text
AttitudeError
-> gain schedule
-> control kernel
-> saturation
-> anti-windup update
-> fault latch/fallback
-> MomentDemand
```

控制器 owner 同时提交 controller state。Allocator 是否单独成为 Runtime Cell 取决于其 clock、fault DecisionAuthority、state 和共享使用情况。

### 9.3 传感器组件

```text
TruthSample
-> sampling/jitter policy
-> bias/noise model
-> dropout/fault mechanism
-> quantizer
-> Measurement
```

随机 stream cursor、bias 和 fault state 都进入 SensorState。噪声模型保持纯算法或嵌入 mechanism；随机源由 Session 计划授权。

### 9.4 导航交班

每个导航器件/算法产生 `NavigationCandidate`。若交班只在一个导航组件内部发生，source ranking 与 handover protocol 作为 mechanism 嵌入。多个独立 producer、外部交班命令和多个消费者共同依赖唯一 estimate 时，建立 `NavigationDecisionCell`；交班 protocol 仍是其内部 mechanism，该 cell 实现唯一 `DecisionAuthority`。

### 9.5 气动、推进和质量模型

查表、插值和域检查通常属于纯 AlgorithmKernel/mechanism。构型选择若只影响单个 owner，可局部嵌入；多个物理模型必须共享同一 revision 时，由专门的 configuration StateOwner/DecisionAuthority cell 发布 `ConfigurationSnapshot`。

### 9.6 模拟故障与退化

单组件故障优先作为宿主 recipe 内的 mechanism：

```text
typed FaultCommand / trigger input
-> FaultMechanism(definition, FaultStateFragment)
-> local fault decision and next fragment
-> normal AlgorithmKernel receives effective parameters/state
-> ordinary output with quality/fault annotation
```

舵机卡死、速率下降、传感器 bias 漂移、dropout、发动机推力损失和执行机构回中等故障都可遵循该结构。FaultStateFragment 随宿主 owner 一起 reset/checkpoint/rollback；故障触发和恢复产生正式 Event/telemetry。

故障作用跨越多个 owner 时，先寻找真实共享物理事实，例如结构损伤、供电母线或液压系统状态，再建立窄 StateOwner cell 发布带 revision 的 physical availability snapshot。一个通用 FaultManager 不能直接改写多个组件内部 state。

模拟故障不会直接调用 DiagnosticPolicy。输入不受支持、fault payload 非法或故障算法数值失败才产生 Diagnostic；已经成功激活的故障属于模型事实。后续轨迹、失控、撞地和终止由普通物理闭环与 evaluator 推导。

## 10. 从 mechanism 晋升为 Runtime Cell

| 判断问题 | 留在宿主内部 | 晋升为 Runtime Cell |
| --- | --- | --- |
| 是否拥有独立 clock/rate/trigger | 与宿主相同 | 独立 |
| 是否拥有独立 reset/checkpoint 边界 | 随宿主 | 需要单独恢复或复用 |
| 是否被多个 owner 共同消费 | 局部值 | 稳定共享 output |
| 是否接受外部 command | 宿主 command 的内部含义 | 自己拥有 DecisionAuthority |
| 是否有独立 failure/termination policy | 宿主统一处理 | 需要隔离或单独 outcome |
| 是否持有外部 resource | 无 | 有独立 lease |
| 是否需要独立替换/配置 | 只是宿主实现细节 | Mission 需要独立选择 |

代码行数、类数量、算法复杂度和复用意愿不参与晋升判断。

晋升后，原 mechanism 可以继续作为新 cell 内部实现。外部只看到 Runtime Cell 的 StateOwner、ports、obligations 和 DecisionAuthority。

## 11. 共享 DecisionAuthority 模式

共享概念需要唯一 owner，并通过 snapshot 暴露 committed 事实：

```text
Commands / Candidate Events
    -> DecisionAuthority Runtime Cell
       [protocol or mode mechanism]
       [owner transition reducer]
    -> committed AuthoritySnapshot(revision)
    -> typed consumers
```

适合该模式的概念包括：

- flight/mission phase；
- navigation-source DecisionAuthority；
- physical configuration；
- control DecisionAuthority handover；
- multi-vehicle coordination state。

`DecisionAuthority` RuntimeCell 只拥有一个内聚概念。它不读取所有组件内部状态，也不演变成全局任务控制器。输入来自稳定 candidate/event/command contract，输出是带 revision 的 typed snapshot。

## 12. 物理构型变化

物理构型会同时改变气动、质量、推进和执行机构能力。目标信息流为：

```text
ConfigurationCommand / sensed completion event
    -> VehicleConfigurationDecisionCell
       [transition protocol mechanism]
       [ConfigurationState owner]
    -> ConfigurationSnapshot { configuration_id, phase, revision, progress }
    -> Aero / Mass / Propulsion / Actuator / Closure consumers
```

每个 consumer 的 ModelDefinition 声明：

- 支持的 configuration ids；
- 参数/资产选择或 state mapping；
- transition 期间的有效域；
- 不支持构型时的 Diagnostic/Outcome；
- 是否需要加入同一 `SolverIslandPlan`。

Compiler 检查每个可达 configuration 的 consumer coverage。运行时 transition 通过一个 owner replacement 和正式 snapshot 提交，避免各组件各自维护字符串 phase。

### 12.1 命令基准 revision

Allocator 形成的 `ActuatorCommand` 携带 `basis_configuration_revision`。若构型在同一 tick 提交新 revision，Actuator owner 按 definition 中声明的策略处理旧命令：

```text
RejectOldCommand | Neutralize | TypedRemap
```

结果写入 `ActuatorCommandDisposition` output/telemetry。该对象属于周期 SampledSignal 语义，不进入 Session command ledger。

### 12.2 连续展开过程

起落架、机翼或整流罩展开可以由 configuration StateOwner cell 的 continuous state + local protocol mechanism 表达。目标 v1 只允许 AtGrid transition 改变 mode/configuration；连续 located event 只形成 estimate telemetry，未来 SegmentTransaction 再承载步内 jump。

### 12.3 分离与实体 activation

v1 把可能出现的分离实体预编译为 inactive entity。构型 transition 产生 `ActivateEntityRequest`，ModelCommit 原子更新 activation 与 configuration revision。未知数量实体和运行期 topology 改写需要未来 `TopologyTransaction`。

## 13. Blueprint 与 authoring metadata

embedded mechanism 可以提供可选 descriptor：

```text
MechanismDescriptor {
  mechanism_id/version
  config_schema
  state_fragment_schema
  local_input/output_schema
  documentation_ref
  composition_constraints
}
```

该 descriptor 服务于 recipe builder、文档生成和蓝图编辑。它不会让 mechanism 成为 Session node。蓝图有两种粒度：

- 普通研究者在 Runtime Cell/ModelDefinition 粒度装配 Mission；
- package 作者在 Runtime Cell Recipe 粒度组合内部 behavior。

两种蓝图最终都经过 Compiler，生成相同 Execution Plan。前端不能在运行中向 cell 注入任意 callback 或修改局部 state fragment。

## 14. Diagnostic、Outcome 与观测

mechanism 返回的 DiagnosticDraft 带局部 attribution path：

```text
runtime_instance_id
component_callsite_id
mechanism_path
algorithm/mechanism version
tick / state_epoch
structured arguments
```

宿主补充 port、phase 和 owner 信息。Session policy 只看到标准 Diagnostic，不依赖具体 C++ 异常类型。

局部 decision、constraint activity 和 convergence 信息先进入宿主 telemetry。只有被跨组件或外部工具稳定消费的值才提升为正式 output/event contract。

## 15. 测试分层

| 层次 | 测试重点 |
| --- | --- |
| AlgorithmKernel | 公式、单位、适用域、数值性质和 reference |
| mechanism | state transition、边界、reset、determinism 和 property |
| Runtime Cell Recipe | state fragment 合成、local pipeline、failure propagation |
| Runtime Cell contract | ports、obligations、owner delta、rate 和 DecisionAuthority |
| compiled plan | binding、DAG、coverage、single writer 和 solver membership |
| closed-loop scenario | 物理/控制结果、transition 和 evidence |

同一 mechanism 的多个宿主需要 contract tests，确保 state fragment namespace、reset 和 Diagnostic attribution 一致。

## 16. 防膨胀规则

1. 不为每种局部工具增加 RuntimeCellProfile switch 或 Kernel obligation。
2. 不建立可写外部状态的通用 callback mechanism。
3. 不让 mechanism 自己解析 Mission、打开文件、启动线程或写日志。
4. 不把局部 pipeline 暴露成全局 service locator 或动态消息总线。
5. 不因多个组件复用同一代码就共享同一 mutable mechanism instance。
6. 不把临时中间量提升为稳定 output；先使用 telemetry。
7. 不在 framework 收纳只服务单个论文或项目的特殊机制。
8. 不让共享 DecisionAuthority 同时管理多个无关概念。
9. 不用 RuntimeCellProfile 名驱动 Kernel 分支；Kernel 只执行 compiled obligations。
10. 新 mechanism family 先用真实宿主证明 state、failure 和组合语义。

## 17. 完成定义

1. 复杂组件能够由 AlgorithmKernel、mechanism、Adapter、Projection 和 owner reducer 组合。
2. 所有 mechanism state 都合并到唯一宿主 StateSchema，并覆盖 initialize/reset/checkpoint。
3. 状态机、滤波、滞回、协议和故障逻辑都不拥有 Session identity。
4. Runtime Cell 的产生由 state/time/DecisionAuthority/failure/resource 边界决定。
5. 常见新算法和内部逻辑变化只修改 package/model SDK，不修改 Kernel。
6. shared phase、navigation-source DecisionAuthority 和 physical configuration 使用窄 StateOwner/DecisionAuthority cell + typed snapshot。
7. blueprint 可以描述 recipe，运行期仍消费静态 compiled composition。
8. mechanism failure 随宿主 transaction 回滚，并有精确 Diagnostic attribution。
9. `framework/model_sdk/behavior` 只包含经多个真实组件验证的稳定工具。
10. 模拟故障通过宿主 FaultStateFragment 或窄物理 StateOwner 表达，未形成可跨 owner 写状态的全局 FaultManager。
11. 纵向案例证明组件内部复杂度增长不会推动全局节点和 Kernel API 同步增长。
