# 路线 R6–R8｜Experiment、Python、LLM、蓝图与实时前端

[上一分册：R3–R5](r3-r5-kernel-and-research.md) · [返回路线总览](../11-roadmap-overview.md) · [下一分册：迁移与治理](migration-governance-and-acceptance.md)

**主线定位**：本分册在已闭合的 Plan、Session、Artifact 和 Workflow 上增加规模与消费者，形成 Experiment、Python/RL、LLM/蓝图和实时前端。所有入口只通过 Authoring、Control 与 Observation 边界接入。

## 1. 这一阶段解决什么

R6–R8 在稳定 Mission/Session/Artifact/Workflow 基础上扩展入口和规模：

- R6 建立 Experiment、多 Session、Python 和 RL；
- R7 建立统一 Control Plane、LLM 提案环和蓝图式编辑；
- R8 建立 command/snapshot 实时边界、交互前端和领域包扩展。

这些阶段不能提前反向修改内核语义。发现底层契约不足时，应回到对应 owner 修正，再继续上层功能。

### 1.1 作为架构压力测试

R6–R8 同时验证 [02](../02-layered-reference-architecture.md) 定义的扩展接缝能否承受规模、入口和部署变化：

| 阶段 | 主要压力 | 变化类别 | 应使用的接缝 | 默认保持稳定的区域 |
| --- | --- | --- | --- | --- |
| R6 | 大批量 case、多语言、RL 高频交互 | D/E | Workflow、Observation、Application | Model Package、Semantic Compiler、transaction 语义 |
| R7 | LLM、蓝图、多作者编辑与审批 | E | Authoring、Control、Application、Compiler boundary | Execution Plan firewall、Kernel、模型状态所有权 |
| R8 frontend | UI/引擎、命令、快照、回放 | E | Application、Observation、Control | Domain contracts、Runtime Cell Recipe、StepTransaction |
| R8 backend | pacing、lockstep、进程隔离 | E；模型可见时间改变时 F-Model | Execution Backend；必要时 `KernelCapability` gate | Compiler IR、模型包、Artifact contract |
| R8 domain | 卫星、火箭、飞机等新领域 | A/C | Model Package、Domain Contract、Graph/Topology | 通用 Kernel obligation set |

每个 stage checkpoint 都要列出本阶段实际修改的接缝、保持未动的分区，以及新增的 `KernelCapability`、`BackendCapability` 或 `PermissionGrant`。若前端需求要求组件暴露内部指针、若领域包要求 Kernel 增加飞行器类别分支、若批量执行要求 Session 持有 case 调度状态，架构压力测试直接判为失败。

软实时 backend 默认属于 `X` 维的 E 类扩展。它可以新增 pacing/deadline policy 与 backend adapter；StepTransaction、state ownership 和 ObservationSeal 语义保持一致。只有 deadline 参与模型可见时间或提交语义时才形成 F-Model 候选。硬实时进入 `BackendCapability` gate，动态拓扑和连续事件跳变分别进入 `KernelCapability` gate。

## 2. R6：Experiment、Python 与智能体训练

### 2.1 前置条件

- Session 可以创建多个独立实例并 reset 或重建；
- `ExecutionPlanDescriptor` 不可变、可 hash，并可在 worker 内 link 为 `ExecutionPlanImage`；
- typed Observation 与 Command descriptor 可查询；
- Run/Case Manifest 和 Artifact Store 可用；
- Workflow Engine 能调度 Session task。

### 2.2 目标

- 参数空间和 case identity 成为一等模型；
- 批量调度不进入 Session Kernel；
- seed 与执行顺序解耦；
- Python 通过稳定 Application Control API 创建、推进和查询 Session；
- RL reset/step、action/observation/reward/termination 语义闭合；
- 多 Session 并行时没有全局 cwd、logger、RNG 和组件状态污染。

### 2.3 R6.1 ExperimentDefinition

包含：

- ResearchQuestion/Hypothesis refs；
- `base_source_ref | base_descriptor_ref`；存在 `CompilePatch` target 时必须提供可重新编译的 source ref；
- compiler、Catalog snapshot、package/asset lock；
- ParameterSpace；
- SamplingDefinition；
- `CaseParameterTarget` 映射；
- optional base RunBinding 与 runtime command template；
- root seed；
- execution/resource policy；
- retry/failure policy；
- Observation/Metric set；
- AggregationDefinition；
- completion/validity；
- expected report outputs。

### 2.4 R6.2 ParameterSpace

支持首批类型：

- explicit values；
- range/grid；
- probability distribution；
- correlated vector；
- categorical model choice；
- asset variant；
- constrained combination。

每个 parameter 都有稳定 id、value type、unit/frame、bounds、physical significance 和 sampling domain。parameter 可以映射到一个或多个显式 target，派生映射必须引用版本化、纯函数式 converter，不能携带任意脚本或命令行片段。

```text
CaseParameterTarget {
  parameter_id
  materialization_kind  // CompilePatch | RunBindingPatch | RuntimeCommandSchedule
  target_id
  converter_id_and_version
  value_type / unit / frame / bounds
}
```

三类 target 的边界固定如下：

| kind | target | 身份和缓存影响 | 典型用途 |
| --- | --- | --- | --- |
| `CompilePatch` | Mission IR 中声明 experiment-overridable 的 typed path | 生成新的 canonical source/IR；可能产生新的 Descriptor/plan hash | 模型选择、资产变体、结构参数、积分与调度策略 |
| `RunBindingPatch` | `RunBindingSchema.field_id` | 只生成新的 binding hash；复用同一 Descriptor/Image | 初始条件、episode seed、time origin、初始 ParameterState、replay/input ArtifactRef |
| `RuntimeCommandSchedule` | `CommandDescriptor.command_id` | 不改变 plan/binding；生成可记录、可回放的 command stream | 阶段命令、故障注入、声明可调参数更新 |

`RuntimeCommandSchedule` 的每个条目必须声明 logical time 或 event trigger、delivery phase、payload schema、late/missed policy 和 command failure policy。调参命令只能落到 `TunableParameterDescriptor -> ParameterUpdateReducer -> ParameterState` 链。

command failure policy 分别覆盖 submission rejection、ledger expiry/supersession 和 committed application rejection/defer；schedule executor 只把 CommandApplicationReceipt 视为模型已处理证据。StepTransaction rollback 后 due command 仍在队列，不能由 case executor提前标记完成。

无 schema 的 JSON patch、同一 target 的多个 `StateOwner`、把 RunBinding slot 绕回 Mission 配置修改、以及无法判定类别的旧 SimFlow patch 都在 case 物化阶段失败。Descriptor-only 的 Experiment 只能使用 `RunBindingPatch` 和 `RuntimeCommandSchedule`。

物化结果是一条可审计记录：

```text
CaseMaterialization {
  case_id
  normalized_parameter_values
  compile_patch_set_and_hash
  descriptor_outcome_ref
  run_binding_and_hash
  command_schedule_ref_and_hash
  derived_seed
  provenance_refs
}
```

`ExecutionPlanImage` 含进程内函数地址与 handle 布局，不进入 portable CaseManifest；manifest 记录 Descriptor hash、package lock 和 worker 的 link fingerprint。

#### 2.4.1 拉偏、扰动与故障 campaign

每个可拉偏量由模型或 RunBinding schema 提供稳定 descriptor：

```text
VariationTargetDescriptor {
  parameter_id
  semantic / value_type / unit / frame
  valid_domain
  materialization_kinds[]
  correlation_group_id?
  target_builder_or_command_id
  provenance_requirement
}
```

新增质量偏差、安装误差、传感器 bias、气动系数缩放或初始状态误差时，只增加 descriptor、typed builder/state 字段和模型测试。Experiment 的 sampler、case identity、worker 和聚合逻辑继续按 ParameterId 工作，不依赖 placement 字符串、JSON path 或 C++ 成员名。

三类时间语义保持分离：

- 模型结构、资产或 `FidelityLevel` 变化使用 CompilePatch；
- 初态、固定 realization 和 episode parameter 使用 RunBindingPatch；
- 阵风时序、故障触发、恢复和调参使用 RuntimeCommandSchedule 或 replay/input Artifact。

相关分布由 ParameterSpace 的 correlated vector 与版本化 converter 物化，CaseManifest 保存原始样本、派生值、单位和 converter version。普通 runtime component 只接收已经物化的 typed state/input/command，不读取 ExperimentId、case index 或采样器。

故障 campaign 分别统计 fault activated、fault rejected、model failure、numerical failure 和 infrastructure failure。舵机卡死后撞地属于成功执行的故障场景和物理终止；故障 payload 不受模型支持才形成 command rejection/Diagnostic。

### 2.5 R6.3 SamplingDefinition

首批方法：

- full factorial/grid；
- Monte Carlo；
- Latin hypercube（验证后）；
- fixed regression cases；
- user-supplied case table。

sampler 有 algorithm id/version/seed。采样结束后先冻结 `CaseParameterTable`；case materializer 再生成逐 case 的 `CaseMaterialization/CaseManifest`。调度重排不改变采样值、target 解析结果或 derived seed。

### 2.6 R6.4 Case identity 与随机性

CaseId 由 ExperimentDefinition hash、normalized parameter values 和 replicate key 派生。component RNG stream 从 root seed + CaseId + entity/model occurrence/noise id 派生。CaseId 不包含 worker、attempt、cache hit 或本地 link fingerprint。

测试：

- 改 worker count 结果不变；
- retry 使用相同 realization；
- 新增无关 case 不改变已有 case seed；
- 同参数多个 replicate 有稳定不同 stream；
- manifest 可重建全部随机输入。

### 2.7 R6.5 Experiment executor

- sample 并冻结 CaseParameterTable；
- validate target，物化三类 case 输入；
- 按 canonical `CompilePatch` hash 分组，仅编译唯一 source/IR 变体；
- 按 Descriptor hash 与 exact package lock 在每个 worker 进程 link 一次 Image；
- 每个 case 创建空 Session，再用自己的 RunBinding initialize，并通过 Command Queue 注入已冻结 command stream；
- binding-only Monte Carlo 共享 Descriptor、Image 和 PreparedModel；
- command schedule 变化不使 Descriptor/Image cache 失效；
- 默认每个 case 创建新 Session；只有 reset 等价性测试通过的 `RunProfile` 才能复用 Session；
- local Session worker pool；
- memory/CPU slots；
- progress/cancel；
- case isolation；
- failure classification；
- resume from manifests；
- aggregate only eligible cases；
- 未来 remote worker 端口，首版不实现。

### 2.8 R6.6 Aggregation

AggregationDefinition 声明：

- required MetricResults；
- inclusion/exclusion by RunOutcome；
- missing/invalid policy；
- statistics/confidence method；
- grouping dimensions；
- pass/fail rules；
- plots/tables；
- sensitivity/ranking（后续）。

物理终止、数值失败、基础设施失败和取消分别统计。

### 2.9 R6.7 Python binding v1

绑定稳定对象：Workspace、Catalog、Compiler、ExecutionPlanDescriptor、process-local linked plan handle、Session、Experiment、ArtifactRef、Diagnostic/Outcome。Python 不直接暴露 ExecutionPlanImage 内的函数表和地址。

要求：

- opaque handles；
- no C++ exception across binding；
- NumPy typed observations；
- long run 释放 GIL；
- callback 频率受限；
- explicit close/context manager；
- async operation query（可选）；
- Windows/MinGW 或选择的 Python 工具链可重复构建；
- wheel/extension build strategy 有 ADR。

### 2.10 R6.8 Python Session 语义

- compile once, create many sessions；
- initialize/reset with schema-validated RunBinding；
- single step / run until；
- command validation；
- snapshot/observation schema；
- failure returns outcome and diagnostic access；
- cancel/dispose；
- memory sink；
- no implicit output directory unless requested。

### 2.11 R6.9 RL Env adapter

- derive ObservationSpace from fields；
- derive ActionSpace from command schema；
- configurable normalization；
- frame skip/action hold；
- reward MetricDefinition；
- terminated/truncated distinction；
- safety action filter；
- deterministic reset；
- info includes diagnostics/metrics；
- Gymnasium compatibility facade（若依赖允许）。

### 2.12 R6.10 VectorEnv

- independent Session per env；
- shared immutable plan/assets；
- batched actions/observations；
- thread/process mode benchmark；
- individual failure isolation；
- partial reset；
- seed independence；
- memory high-watermark；
- `TrainingProfile` 默认不产生隐式记录文件；
- 评估使用冻结的 `RunProfile`，并可启用完整证据输出。

### 2.13 R6.11 模型 Artifact

训练产物记录：

- policy model file/hash；
- training code/environment；
- base plan/package/asset versions；
- reward/observation/action definitions；
- seed and case distributions；
- checkpoints；
- evaluation run set；
- metrics and limitations。

策略部署到 Session 前经过 model adapter 和 evaluation gate。

### 2.14 R6 验收矩阵

| 场景 | 验收 |
| --- | --- |
| 100 case local sweep | identities、seeds、manifests 完整 |
| 100 case initial-condition sweep | Descriptor 编译一次，worker 内 Image link 一次，100 个 binding hash 独立 |
| model/asset variants | 只按唯一 CompilePatch/plan hash 分组编译 |
| command schedule variants | plan/binding hash 稳定，command stream hash、ledger receipt 与 committed application receipt 可追踪 |
| new perturbation target | 只新增 model/RunBinding descriptor 与 fixture；sampler/executor/Session 无修改 |
| correlated perturbations | samples、converter version、derived values、units 和 seeds 可完整重建 |
| actuator-stuck campaign | activated/rejected/model failure/physical crash 分类分离，证据链闭合 |
| ambiguous patch | case 在创建 Session 前失败，并指出 parameter、target 和候选类别 |
| worker count change | case values与声明确定性保持 |
| one case fails | others follow policy，aggregate 分类正确 |
| resume | completed cases 不重跑，cache/manifest verified |
| two Python Sessions | state/RNG/output 隔离 |
| reset same seed | same initial/episode observations |
| VectorEnv | partial reset 和 failure isolation |
| `TrainingProfile` | 无隐式文件 I/O，吞吐有基线 |
| 评估 `RunProfile` | full evidence + fixed test set |
| Python buffer | lifetime/shape/dtype 正确 |

### 2.15 R6 退出条件

- Experiment 吸收 SimFlow 中批量身份、seed、调度和聚合语义；旧 SimFlow schema 与运行入口删除；若历史 case 确有复用价值，只提供输出 `ExperimentDefinition` 的一次性离线转换器，转换器必须把每个 patch 归入 CompilePatch、RunBindingPatch 或 RuntimeCommandSchedule，含糊映射直接报告失败；
- Python 和 CLI 共用 compiler/session services；
- 多 Session 已通过隔离和性能测试；
- 一个简单智能体可以稳定训练和复现实验；
- 策略评估生成完整 Evidence Bundle；
- R7 可以只使用 Control API DTO，不触碰内部 C++；
- R6 只通过 Workflow、Observation 与 Application seams 扩展，Kernel 中没有 case、sampler、agent 或 Python 分支。

## 3. R7：Control Plane、LLM 辅助与蓝图 Studio

### 3.1 前置条件

- Catalog、Compiler、Session、Experiment、Workflow、Artifact 都有稳定 application service API；
- stable ids/schema/diagnostics 可序列化；
- command permissions 和 audit 基础可用；
- 至少一条纵向研究模板可以被自动化调用。

### 3.2 目标

- 所有交互入口共享命令、查询和事件；
- LLM 以 typed proposal 工作；
- 蓝图节点和 socket 来自 catalog；
- Simulation Graph 与 Workflow Graph 分离；
- 自动生成的变更经 compile/diff/policy/approval；
- 每次自动化操作进入 audit Artifact。

### 3.3 R7.1 Control API v1

稳定 command groups：

- workspace/package/catalog；
- mission compile/validate/diff；
- session create/run/step/cancel；
- experiment create/run/resume；
- workflow start/approve/cancel；
- artifact query/export；
- diagnostics explain/remediation。

稳定 query DTO：CatalogView、GraphView、PlanView、OperationStatus、SessionSnapshot、ExperimentStatus、LineageView。

### 3.4 R7.2 Operation 与 audit

- OperationId/request id/idempotency；
- Submitted/Running/WaitingApproval/Completed/Failed/Cancelled；
- actor、permissions、input refs；
- commands/events/outcomes；
- plan/source revisions；
- approvals；
- audit Artifact；
- retention/security。

CLI 和 Python 迁移到同一 API 后才冻结 v1。

### 3.5 R7.3 Permission/Policy

先支持本地单用户 `PermissionGrant` action：Read、Propose、Compile、ExecuteLocal、ExternalTool、Export。每个 operation 声明所需 grant，授权范围同时限定 actor、resource、scope、expiry 与 approval ref。

高风险 gate：

- 修改物理参数/模型；
- 降低 maturity/validity policy；
- 允许 extrapolation/waiver；
- 运行外部工具；
- 写出 workspace；
- 删除/覆盖 Artifact；
- 发布正式报告。

### 3.6 R7.4 LLM context service

只提供经选择的结构化上下文：

- architecture/glossary excerpts；
- Catalog/schema/template；
- current source/IR diff；
- diagnostics/evidence；
- Artifact summaries；
- permission/policy；
- operation history。

不把整个工作区、隐式 secret 或内部指针状态交给模型。

### 3.7 R7.5 Proposal model

首批 proposal intent：

- 起草 ResearchQuestion；
- 提交 `MissionSourcePatch`，其 typed target 可以落到 Mission 结构、ParameterSet 或 ObservationPlan；
- 实例化 WorkflowDefinition；
- 提议 Figure/Report specification；
- 选择 DiagnosticCodeSpec 登记的 remediation action。

每个 `ResearchProposal` 有 target revision、typed operations、assumptions、unresolved items、expected impact 和 requested `PermissionGrant`。

### 3.8 R7.6 自动验证环

流程：

1. validate proposal schema；
2. apply to temporary source revision；
3. compile/dry-run；
4. show semantic diff；
5. list assumptions、diagnostics、maturity/validity changes；
6. estimate resources/storage；
7. policy evaluate；
8. request approval；
9. commit source revision；
10. execute operation；
11. attach outcome/audit。

任何一步失败都可返回 LLM 继续修订，不能绕过 compiler。

### 3.9 R7.7 LLM 防错测试

- 不存在 component/field；
- unit 错误；
- form family/frame mismatch；
- required physical field omitted；
- unsafe path/tool command；
- maturity downgrade；
- stale source revision；
- prompt injection in Artifact text；
- unauthorized export；
- repeated request id；
- fabricated metric conclusion。

权威判断来自 schema/compiler/policy/Artifact，模型文本不能覆盖。

### 3.10 R7.8 Blueprint data model

- semantic simulation graph；
- workflow Artifact graph；
- separate layout/annotation graph；
- graph revision/id；
- node definition refs；
- port/edge ids；
- parameter bindings；
- subgraph/template instance；
- diff/merge；
- source/IR round-trip。

### 3.11 R7.9 Studio MVP

MVP 先做：

- Catalog palette/search；
- mission tree + graph viewer；
- schema-driven property editor；
- port connection；
- live compile diagnostics；
- phase/rate table；
- plan graph preview；
- source diff；
- run button和operation status；
- artifact/metric viewer。

Workflow graph 编辑和 report design 可在后续小版本加入。

### 3.12 R7.10 双向一致性

- Source -> Graph -> Source 保留 semantic equality；
- layout 不改变 plan hash；
- unknown/custom project fields 保留或清楚诊断；
- source conflict 要求 merge，不静默覆盖；
- LLM patch 与 GUI edit 使用相同 revision model；
- CLI 可以输出相同 semantic diff。

### 3.13 R7 验收矩阵

| 场景 | 验收 |
| --- | --- |
| CLI/Python/Studio compile | 相同 plan hash |
| LLM invalid type | proposal/compile 失败，不执行 |
| LLM physical change | diff + approval + audit |
| stale revision | conflict，不覆盖新编辑 |
| graph wrong edge | port diagnostic 定位 socket |
| source round-trip | semantic equality |
| layout move | plan hash 不变 |
| unauthorized tool | policy reject |
| operation retry | idempotent |
| result explanation | 引用真实 Metric/Artifact |

### 3.14 R7 退出条件

- Control API 被至少 CLI、Python、Studio 三个入口使用；
- LLM 所有写操作都是 typed proposal；
- compile/diff/approval/audit 链不可绕过；
- 蓝图可编辑一个真实 mission 并生成相同 IR；
- diagnostics 在图节点/端口定位；
- 权限和 prompt injection 测试通过；
- 尚未冻结动态插件或远程公共 API；
- LLM、蓝图和 GUI 都停留在 Plan Firewall 外侧，执行路径只接收已编译计划与受控命令。

## 4. R8：实时仿真、游戏前端与领域扩展

### 4.1 前置条件

- Session pause/step/command/snapshot 稳定；
- Observation backpressure 与 display criticality 可配置；
- Control API 可处理长 operation 和实时 Session query；
- 多 Session/IPC DTO 有性能基线；
- 至少一个 3DoF 或 6DoF domain package 成熟。

### 4.2 目标

- renderer 与内核通过 Command/RenderSnapshot 协作；
- soft real-time、interactive lockstep 可用；
- ImGui 提供研究调试面板；
- 选择 UE 或 Godot 完成一个端到端原型；
- GNC 算法可以成为交互玩法，同时 fidelity/证据等级清楚；
- 领域差异通过 package/template 扩展。

### 4.3 R8.1 RenderSnapshot contract

- entity id/type；
- simulation tick/time；
- transform with frame；
- linear/angular velocity（可选）；
- visual configuration；
- animation/control surface values；
- event markers；
- display telemetry；
- interpolation flags；
- validity/schema version。

snapshot 是 immutable published data，前端不持有组件内存。

### 4.4 R8.2 Interactive Command contract

- control input；
- mission command；
- pause/resume/step；
- 发往明确 `DecisionAuthority` 的 typed mode/source request；
- parameter tuning（仅 tunable）；
- 预声明实体的 activation/scenario command（仅限 plan 已编译的 topology）；
- timestamp/effective tick；
- expiry/sequence；
- `CommandSubmissionOutcome`、`CommandLedgerMaintenanceReceipt` 与 committed `CommandApplicationReceipt`。

输入映射、deadzone、scaling 和 rate limit 由 adapter definition 记录。

### 4.5 R8.3 Soft real-time scheduler

- wall pacing；
- lag measurement；
- catch-up budget；
- no physics step drop 默认；
- display snapshot drop/decimate；
- deadline miss diagnostic/metric；
- pause/cancel safe point；
- run manifest 记录 pacing；
- 可复现 `RunProfile` 可切回 unpaced。

硬实时和 HIL 需要独立路线，不纳入 R8 完成门。

### 4.6 R8.4 IPC/embedded spike

用同一小场景比较：

- embedded latency/build complexity/crash coupling；
- local IPC serialization/throughput/debug；
- snapshot frequency 与 size；
- command latency；
- engine toolchain compatibility。

ADR 选择首个前端模式，保留另一模式的 contract compatibility。

### 4.7 R8.5 ImGui research console

- Session controls/state；
- live time series；
- entity/state inspector；
- phase/component timing；
- diagnostics list；
- event timeline；
- command/tuning panel；
- snapshot 3D display（可选）；
- artifact capture/bookmark。

ImGui 是验证 Control/snapshot 的低成本入口，优先于大型游戏场景。

### 4.8 R8.6 UE 或 Godot 单前端原型

选择标准：

- 现有团队熟悉度；
- C++/GDExtension/IPC 集成成本；
- 坐标和大世界需求；
- 可视化/物理资产；
- 跨平台；
- 调试和打包；
- license。

原型只选择一个引擎，完成：

- 一架飞行器/导弹和目标；
- command input；
- render snapshot；
- soft real-time/lockstep；
- pause/replay；
- event/telemetry UI；
- 一个算法替换或调参玩法；
- outcome/score Artifact。

### 4.9 R8.7 Lua ScriptComponent spike

- declared ports/state/config；
- sandbox；
- deterministic RNG；
- instruction/time budget；
- reset/checkpoint；
- runtime diagnostics；
- paused hot reload with plan revision；
- 一段离散制导/任务逻辑案例。

性能不满足时保留 Lua 用于任务逻辑，飞控/动力学继续 C++。

### 4.10 R8.8 领域包路线

优先顺序由真实项目决定，建议：

1. 整理当前 missile/3DoF/6DoF 为首个 domain package；
2. 选择一个卫星、火箭或飞机真实项目建立 candidate package；
3. 只补其所需的 time/event/form/contracts；
4. 经过两个场景后提取共用 contract、model 或通用 operator；
5. 另一个领域通过复用测试验证 Kernel 无硬编码。

每个包交付 contracts、components、assets、templates、metrics、workflows、verification 和 UI palette。

### 4.11 R8.9 游戏 `FidelityLevel`

定义：

- Engineering；
- ResearchInteractive；
- TrainingFast；
- GameplaySimplified。

`FidelityLevel` 声明模型成熟度、允许简化和结果用途，配套 `RunProfile` 声明数值模式、数据记录与实时预算。UI 同时显示两者，Artifact/score 也记录。

### 4.12 R8.10 Replay

- command stream；
- event stream；
- plan/assets/seed lock；
- optional snapshots；
- deterministic re-simulation；
- display-only replay；
- divergence metric；
- version mismatch diagnostic。

### 4.13 R8 验收矩阵

| 场景 | 验收 |
| --- | --- |
| renderer 60 FPS / sim 100 Hz | snapshot 插值、无直接 state 写 |
| display stall | physics 按 policy 继续，drop 计数 |
| command burst | sequence/expiry、`PermissionGrant` 与目标 `DecisionAuthority` 正确 |
| pause | committed boundary 一致 |
| replay | 达到声明确定性等级 |
| engine crash（IPC） | Session 可隔离/记录 |
| Session failure | 前端展示 structured diagnostics |
| Lua over budget | Diagnostic + policy action |
| fidelity switch | 重新编译 plan/version 记录 |
| domain template | compiler closure + evidence bundle |

### 4.14 R8 退出条件

- Command/RenderSnapshot 成为所有实时前端唯一物理交互边界；
- soft real-time 和 lockstep 有测试与性能数据；
- ImGui 可完成日常运行监控；
- 一个游戏引擎原型闭合操作、显示、回放和结果；
- 至少一个领域 package 完成 stable/candidate 分层；
- 游戏简化与工程结果用途明确区分；
- 动态插件、远程服务和硬实时由新 ADR/路线决定；
- 新领域通过 package/contract 扩展，Kernel 未出现 missile、satellite、rocket、aircraft 或 engine-specific dispatch。

## 5. R6–R8 风险与缓解

| 风险 | 缓解 |
| --- | --- |
| Python 暴露内部对象导致 API 锁死 | 只绑定 Control DTO/opaque handles |
| RL 吞吐驱动破坏研究语义 | 独立 `TrainingProfile`，评估 `RunProfile` 固定完整证据 |
| LLM 自动执行危险修改 | typed proposal + compile + policy + approval |
| 蓝图形成第二种 mission 语义 | semantic graph 编译到同一 IR |
| UI 需求反向侵入 Kernel | Command/RenderSnapshot ports |
| 同时支持多个引擎耗散精力 | R8 只选择一个完整原型 |
| domain pack 过早泛化 | 真实项目优先，两场景后提取 common |
| Lua/Python callback 破坏实时性 | budget、`RunProfile`、out-of-process/隔离执行 |
| 远程/动态安全复杂度 | 延后到真实部署需求 |

## 6. R6–R8 总完成定义

1. Experiment case、参数、seed、失败和聚合全部结构化。
2. Python 能可靠创建多个 Session，并支持 RL reset/step。
3. 训练策略与评估证据形成 Artifact 谱系。
4. CLI、Python、LLM、Studio 共用 Control Plane。
5. LLM 所有修改经 proposal、compile、diff、approval 和 audit。
6. 蓝图语义图与 Mission IR round-trip，布局独立。
7. 实时前端只使用命令和不可变快照。
8. soft real-time、lockstep、pause 和 replay 可验证。
9. 领域能力由 package 组合，Kernel 不含具体飞行器类别分支。
10. 一个交互原型证明 GNC 算法设计可以成为核心玩法，同时保持 fidelity 声明。
11. Experiment、LLM、蓝图和实时命令都沿 Operation Authority 形成 ledger/receipt/audit，并只用 typed refs 调用 Plan、Model 与 Artifact owner。
