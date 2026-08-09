# 01｜当前架构深度审阅

[上一册：愿景与架构宪章](00-vision-and-architecture-principles.md) · [返回总索引](README.md) · [下一册：系统架构蓝图](02-layered-reference-architecture.md)

**主线定位**：本册把现行源码、测试和文档映射到“意图—计划—运行—证据—工作流”主线，给出保留、修正和退出证据。它为 [02](02-layered-reference-architecture.md) 与 [11](11-roadmap-overview.md) 提供迁移起点，不定义目标架构的最终契约。

## 1. 审阅结论

当前 GNCZMKN 已经建立了有价值的骨架：节点有清楚的 placement，运行循环有稳定的发布态语义，连续系统同步提交下一状态，SimFlow 留在单次运行内核之外，项目私有能力优先沉淀在 `user/`。这些设计值得保留。

当前主要问题来自架构机制的成熟度不均衡。若干区域已经拥有清晰概念，另一些区域仍依靠字符串、裸数值、日志、异常和约定连接。局部代码可以完成任务，跨层契约却缺少统一语言，最终呈现出“模块都有、体系未闭合”的观感。

这些现象共享一个更深层根因：架构长期按“当前有哪些对象和功能”组织，尚未建立独立于产品清单的需求变化模型，也没有明确区分 Design/Plan、Model、Operation 与 Artifact 四类提交权。于是新增功能通常从某个现有类、manager 或文件格式向外生长；配置、运行、工作流、记录和前端各自形成局部控制流，缺少统一的 grammar、proof、closed operator、commit 和 typed handoff 判据。

因此，审阅先把问题提升为三项架构缺口：

1. 缺少一组相互可区分的变化坐标，无法稳定回答概念、拓扑、状态、时间、信息权威、表示和执行环境分别改变了什么；
2. 缺少权威域，无法稳定回答某项变更可以提交设计、模型、操作或证据中的哪类事实；
3. 缺少扩展闭包，接口存在并不等于能力获得支持，需求仍可能在运行期靠 callback、全局对象、字符串或隐藏副作用补齐。

下列六项是该共同根因在当前源码中的直接表现：

1. 数学存储类型承担了过多领域语义，单位、坐标、方向和时间有效性没有成为类型或元数据契约。
2. C++ 继承关系和 RTTI 被同时用于实现接口、发现能力和表达领域兼容性，接口层缺少分级与版本策略。
3. Mission 仍接近对象创建脚本，装配边、端口、schema、默认值来源和编译结果没有形成稳定 IR。
4. `Simulator` 同时拥有调度、积分、记录、总结、终止和生命周期，失败后的资源收尾与结果表达没有闭合。
5. 日志文本、异常文本、布尔返回和结果结构各自表达失败，缺少统一的诊断与处置模型。
6. 单次运行被当成主要交付物，研究问题、批量实验、分析产物、外部工具和报告之间还没有权威谱系。

## 2. 审阅范围与证据

本轮重点核对以下主线：

```text
src/runner.cpp
-> SimulationBuilder
-> MissionAssembler
-> ValidationPipeline
-> PreparationPipeline
-> Simulator
```

同时审阅了数学与控制库、核心接口、NodeFactory、AssemblyContext、配置解析、自动记录、总结输出、SimFlow 和项目侧 6DoF 类型。

关键源码证据：

- [simulation_builder.hpp](../../reference/legacy/source-index.md#current-execution-chain)
- [mission_assembler.hpp](../../reference/legacy/source-index.md#current-execution-chain)
- [validation_pipeline.hpp](../../reference/legacy/source-index.md#current-execution-chain)
- [simulator.hpp](../../reference/legacy/source-index.md#current-execution-chain)
- [node_factory.hpp](../../reference/legacy/source-index.md#current-execution-chain)
- [assembly_context.hpp](../../reference/legacy/source-index.md#current-execution-chain)
- [config_manager.hpp](../../reference/legacy/source-index.md#current-execution-chain)
- [common/math](../../reference/legacy/source-index.md#legacy-source-index)
- [libraries](../../reference/legacy/source-index.md#legacy-source-index)
- [interfaces](../../reference/legacy/source-index.md#legacy-source-index)

以下判断区分“当前事实”和“目标建议”。当前事实来自 2026-07 工作树；目标建议由后续分册展开。

## 3. 当前架构值得保留的部分

### 3.1 节点职责与 placement

`Form`、`Environment`、`vehicle.common`、`Input`、`Process`、`Output`、`Interaction`、`Infrastructure` 和 `Output/Logger` 已形成有用的责任地图。尤其有三条边界很重要：

- `Form` 拥有连续状态和导数方程；
- `Interaction` 闭合 truth、环境、命令和物理能力；
- `vehicle.output` 承载气动、质量、推进、执行机构等运行时物理能力。

这些边界应继续作为领域装配的基础。后续工作重点是把它们从 placement 约定提升为可机读的端口和能力契约。

### 3.2 生命周期分段

`configure -> bind -> prepare -> initialize` 已经把配置读取、依赖绑定、静态资产准备和运行态初始化分开。该顺序适合研究框架，也为缓存、并行准备和故障定位提供了起点。

当前缺口集中在阶段结果类型、幂等性验证、失败回滚和阶段上下文。生命周期本身应保留。

### 3.3 时间切断点

固定步长循环以周期开始发布态为权威读视图：

```text
publish(t_k)
-> before_step(t_k)
-> discrete update(t_k)
-> record(t_k)
-> termination(t_k)
-> synchronized integration
-> after_step(t_{k+1})
```

这条语义清楚、可测试，也能解释 CSV 每一行的含义。后续需补齐事件、取消、异常和输出提交语义，不能轻易改变上述基础顺序。

### 3.4 同步独立积分和显式连续组

所有连续系统先计算候选下一状态，再统一 `setState()`，避免注册顺序造成串行污染。需要 RK 子步共享候选状态的模型通过 `IContinuousGroup` 显式声明耦合边界。这是合理的最小设计。

后续要解决状态布局、容差策略、组内雅可比/稀疏性、积分结果和非有限值处置，当前同步提交原则应继续保持。

### 3.5 SimFlow 边界

SimFlow 负责仿真前物化，单个 case 仍沿普通 mission 运行。这种边界保证单 case 可独立复现，也避免批量语义进入 `Simulator`。未来 Experiment 与 Workflow 应沿用这条原则。

### 3.6 项目优先的晋升路径

`user/<project>` 允许论文、试验和私有模型快速迭代，framework 只收纳稳定契约。这符合实验室级工具的维护现实。后续模型包体系应增强可发现性、版本和依赖管理，不应削弱项目工作区的低门槛。

## 4. 数学与数值底层审阅

### 4.1 三套基础表示并存

当前至少存在三套相邻表示：

| 表示 | 位置 | 主要用途 | 风险 |
| --- | --- | --- | --- |
| Eigen 别名 `gnc::math::Vector3/Matrix3` | `common/math/eigen_types.hpp` | framework 新代码 | 只表达维度，无法表达坐标和单位 |
| 自定义 `gnc::math::Quaternion` | `common/math/quaternion.hpp` | 数学库 | 与 Eigen 四元数并存，旋转约定需人工判断 |
| `gnc::Vector3d/Quaterniond/Matrix3d` | `common/math_types.hpp` | 旧接口和格式化 | 功能弱、语义宽泛、形成迁移负担 |

项目侧 6DoF 类型还直接使用 `Eigen::Quaterniond`。同一个姿态概念因此可能经历三套表示和多次转换。

**影响**：类型兼容不能证明物理兼容；四元数系数顺序、主动/被动旋转和 frame direction 可能在适配处漂移；公共接口难以形成稳定语言绑定。

### 4.2 坐标、方向和单位主要依靠名字

`Vector3` 可同时表示 ECEF 位置、NUE 速度、机体系角速度、力和力矩。`interfaces/data_types.hpp` 甚至把 position 注释为“ECEF 或本地坐标”。这些类型在编译器看来完全相同。

**影响**：组件连接能通过 C++ 类型检查，仍可能形成坐标错接；自动蓝图和 LLM 无法仅凭 schema 判断连接是否合法；数据记录需要从字符串列名猜测单位。

### 4.3 四元数与旋转约定未集中治理

数学库、form 类型和项目类型分别声明 `body_to_inertial`、`body_to_nue`、`body_to_frame`。自定义四元数的 rotate 顺序、Eigen 操作和 form 中的导数公式需要开发者逐处核对。

**影响**：姿态方向错误常表现为数值稳定但物理错误，测试若只覆盖单轴或单位四元数，很难发现。

### 4.4 数值失败语义分散

当前数值 API 采用多种策略：

- 求根与优化返回 `converged`；
- 线性代数部分函数抛异常，部分返回 `bool`；
- 统计函数在空输入或样本不足时可能返回 `0.0`；
- PID 在 `dt <= 0` 时返回 `0.0`；
- 插值默认 Clamp，调用方无法知道值曾越界；
- ODE 结果只有较薄的 `success` 信息。

**影响**：物理关键计算可能把失败转换成看似合法的零值或边界值；不同算法调用方需要掌握不同处置规则；诊断层无法稳定分类。

### 4.5 两套积分语义

`common/math/calculus.hpp` 提供 RK4、RK45 和完整 ODE 积分流程；运行时又通过 `IIntegrator`、Euler 和 RK4 实现固定步进。两套代码处在不同抽象层，却没有明确命名和共享策略。

**影响**：容差、状态检查和算法修复可能只落在其中一套；研究分析与主仿真可能使用名称相同、行为不同的积分器。

### 4.6 控制库混合模型、状态与推进

`StateSpaceModel` 同时保存矩阵、内部状态，并提供离散和连续推进；PID、滤波器也自行保存状态和 reset 语义。

**影响**：同一模型参数难以安全共享给多个 Session；并行训练时容易误用共享实例；离线分析模型与运行时状态实例边界含糊。

### 4.7 测试覆盖不均衡

当前测试大量覆盖具体 form、环境、自动记录和项目组件，基础求根、优化、统计、插值、滤波与状态空间算法缺少直接系统性测试。

**影响**：底层算法一旦调整，集成测试很难定位误差来源；算法精度、收敛域和跨工具一致性无法形成可信声明。

## 5. 接口层审阅

### 5.1 接口分类混杂

`framework/include/gnc/interfaces` 同时包含连续系统、积分器、可观测字段、记录 sink、终止评价和总结输出。领域接口散落在 environment、form、vehicle/input、vehicle/process、vehicle/output 等目录。

目录体现业务位置，却没有显式区分以下契约层级：

- 数值算法接口；
- 领域数据和值对象；
- 组件输入输出端口；
- 运行时能力；
- 应用控制接口；
- 工作流和 Artifact 接口；
- 跨语言与进程边界。

**影响**：开发者容易把任意纯虚类都视为同等稳定的公共 API；接口依赖方向和版本策略难以治理。

### 5.2 能力发现依赖 RTTI

NodeFactory 保存 `std::type_index` 和由 C++ 类型名生成的接口名称，NodeRegistry 通过动态类型发现能力。这对单进程静态链接可用，却无法成为稳定的 Mission、Python、动态包或网络协议身份。

**影响**：编译器和前端只能看到不稳定的实现类型；接口改名、编译器差异和 ABI 变化难以控制。

### 5.3 依赖绑定边没有持久化

`AssemblyContext::bind` 把解析结果写入组件裸指针。AssemblyGraph 保存 scope 与节点归属，实际的 provider-consumer 边没有成为 Execution Plan 中的权威对象。

**影响**：无法完整绘制闭环；无法在运行前检查端口基数、采样率、单位、frame、版本和循环；运行后也难以解释某个输出来自哪个 provider。

### 5.4 数据时效和质量缺失

现有 truth、navigation、guidance、actuator 等接口通常返回一个 struct。struct 很少携带 sample time、valid time、sequence、freshness、quality、source 和 covariance。

**影响**：多速率系统只能依靠调度顺序和成员命名理解数据新旧；导航交班和传感器失效难以进行通用闭环论证。

### 5.5 I/O 和格式化进入核心接口

`ISummaryObserver::writeSummary(std::ostream&)` 把领域总结与文本格式绑定；`IRecordSink` 以 `vector<double>` 写行，`IObservable` 只暴露名称和 `double` getter。

**影响**：多类型数据、单位、质量标记、schema 版本和列级谱系无法表达；写失败无法可靠传播；报告模板被迫解析自由文本。

## 6. 组件目录与装配审阅

### 6.1 注册元数据不足

NodeFactory 当前记录 type id、category、origin、role、discrete phase、form family 和接口类型。它还缺少：

- 组件与接口语义版本；
- 配置 schema；
- 输入输出端口定义；
- 依赖基数和选择规则；
- 支持的坐标、单位和时效模型；
- 确定性、线程安全、可重入性和实时安全声明；
- 模型成熟度、验证证据和许可证；
- 资产需求、资源预算和平台约束。

**影响**：`--list-components-verbose` 能列出类型，仍不足以驱动自动配置、蓝图连线、兼容性检查和 LLM 设计。

### 6.2 placement 校验强于语义校验

当前 ValidationPipeline 能检查 scope、role、phase、form family 和部分依赖。只要两个节点共享 C++ 接口，物理语义仍可能不一致。

已观察到一个典型风险：部分 framework 3DoF process 类型硬编码 ECEF/NUE 语义，Cartesian form 也复用相同接口。类型连接成功，坐标语义仍需人工保证。

### 6.3 中央注册与构建发现耦合

builtin 注册集中维护，项目组件通过 CMake 扫描包含注册宏的头文件并重新配置构建。单 active project 以编译期方式切换。

**影响**：项目复用主要依赖复制；组件元数据难以离线读取；每次新增组件都牵动构建；未来多语言和蓝图工具无法在不加载 C++ 的情况下发现 catalog。

### 6.4 生命周期返回语义薄弱

`configure`、`bind`、`prepare`、`initialize` 和 `finalize` 多数返回 `void`。外层通过捕获异常拼接文本。PreparationPipeline 会继续准备其他组件，但结果只有字符串列表。

**影响**：失败主体、稳定代码、相关位置、可重试性和已完成清理无法统一表达；部分阶段能聚合错误，部分阶段立即退出。

## 7. Mission 与配置审阅

### 7.1 Mission 承担过多职责

当前一个 JSON 同时描述：

- 场景和飞行器；
- 组件装配；
- 组件参数；
- 运行步长与时长；
- 输出选择；
- 终止与总结。

这些内容变化频率不同，也由不同角色维护。持续扩展后，一个文件会同时承担复用、覆盖、版本迁移和工具生成，复杂度快速上升。

### 7.2 缺少 schema version 和显式迁移

Mission 顶层当前没有完整版本编译链。旧访问器仍以 deprecated API 保留在 ConfigManager 中。

**影响**：输入兼容依赖分支和错误文本；无法在运行清单中证明使用了哪一版语义；自动工具难以稳定生成配置。

### 7.3 宽松读取与严格读取并存

`ConfigNode::asDouble/asInt/asString` 在类型不匹配时返回默认值，`asInt` 对小数直接截断；`ConfigReader` 提供较严格读取。builtin 通常要求严格，项目组件仍可能直接使用宽松 API。

**影响**：字段拼错、类型错误或小数写入整数配置时可能静默改变实验。

### 7.4 加载失败可能保留旧 config

`loadFromFile` 在新文件解析或预处理失败时清空 source path，却未在所有失败路径清空旧 `config_`。调用方若忽略 `false`，可能继续读取上次成功内容。

**影响**：交互工具和长生命周期进程存在陈旧配置风险，且运行清单可能记录错误来源。

### 7.5 include 与覆盖缺少来源映射

当前预处理器支持 `$include` 和多种路径 scheme，这是有用能力。编译结果没有完整 source map，字段最终值来自哪个文件、哪一层覆盖无法统一查询。

**影响**：错误只能指向逻辑 JSON path；配置 diff、LLM 提案和审计无法展示来源链。

## 8. 仿真内核审阅

### 8.1 Simulator 职责过宽

`Simulator` 当前同时负责：

- 节点 registry；
- 执行元数据；
- 直接绑定回退；
- 初始化与 finalize；
- 发布和离散调度；
- 连续系统分组与积分；
- 终止判断；
- 自动记录；
- summary 写出；
- 生命周期 phase。

这使一个类同时跨越领域执行、基础设施和应用编排。任何新时间模式、输出后端、取消或 checkpoint 都会继续增加条件分支。

### 8.2 生命周期状态不完整

ExecutionPhaseManager 只有 `NotStarted/Initializing/Running/Finalizing/Completed`。非法转换只写错误日志并返回，没有 Failed、Cancelling、Cancelled、FinalizationFailed 等状态。

**影响**：外部调用方不能可靠判断 Session 是否仍可运行；异常发生时状态可能停留在 Running；日志和实际对象状态可能分叉。

### 8.3 run 缺少异常安全的统一收尾

正常路径会停止 logger、写 summary、finalize 节点。任一 update、积分、记录或 summary 抛异常时，这些动作不一定执行。

**影响**：输出文件可能未关闭，partial run 缺少状态标记，节点资源未释放，后续运行难以判断可否复用。

### 8.4 输出和总结是具体成员

AutoDataLogger 与 SimulationSummary 被 Simulator 直接持有或调用。

**影响**：内核依赖 CSV/文件系统语义；嵌入 Python、内存采样、实时流或无文件运行需要改动内核；输出失败与物理失败难以独立处置。

### 8.5 调度模型只表达整数倍速率

`rate_hz` 必须形成整数 step interval，这条规则清晰且应保留为当前模式。系统还没有显式表达 offset、deadline、jitter、sample-and-hold、事件触发、不同 clock domain 和实时超限策略。

**影响**：导航器件链路、多速率控制和实时前端只能通过组件内部逻辑补偿，难以分析调度闭合。

### 8.6 积分结果缺少质量信息

运行时 `IIntegrator::step` 直接修改 state，没有接受/拒绝、误差估计、评估次数、步长建议、非有限值位置和失败原因。

**影响**：引入自适应或刚性积分器会迫使接口重构；当前 RK4 遇到 NaN 时只能在更远处发现。

## 9. 诊断、异常和可靠性审阅

### 9.1 四套失败通道并行

| 通道 | 示例 | 局限 |
| --- | --- | --- |
| 异常 | `throw std::runtime_error(text)` | 无稳定 code，边界和可恢复性不清楚 |
| 布尔返回 | config load、sink open | 丢失原因或依靠同时写日志 |
| 字符串列表 | build/preparation errors | 无结构字段、位置和因果链 |
| 日志 | phase、output、summary | 调用方无法机器处理 |

同一故障可能只走一种通道，也可能先写日志再返回 false，最后由上层生成另一段错误文本。

### 9.2 缺少统一处置策略

系统尚未显式定义：

- 哪些问题应聚合后一次报告；
- 哪些问题必须立即停止；
- 哪些 warning 可以升级为 error；
- 数值越界是否允许 clamp、降级或继续；
- 输出失败是否终止仿真；
- finalize 失败如何与原始失败合并；
- 批量 case 的失败如何隔离；
- 外部工具失败是否可重试；
- 用户取消如何区别于运行失败。

### 9.3 缺少稳定诊断身份

错误文本已经尽量携带 mission path，这是良好起点。系统仍缺少 `code`、`category`、`phase`、`subject`、`source span`、`cause`、`remediation`、`related diagnostics` 等结构。

**影响**：测试只能匹配文本片段；文案改动可能破坏自动化；LLM 和前端无法针对问题类型给出可靠操作。

### 9.4 缺少运行结果总模型

当前 termination reason 是字符串，正常完成、条件终止、取消、初始化失败、数值失败、I/O 失败和内部缺陷没有进入统一 `RunOutcome`。

**影响**：Experiment 统计会混淆有效失败样本与基础设施故障；报告无法明确“仿真结论无效”的原因。

## 10. 数据、观测和证据审阅

### 10.1 Observable 过度简化

`ObservableField` 只有名称和 `double` getter。Vector/Quaternion 依靠 helper 展开为多个标量。

**影响**：shape、dtype、单位、frame、插值语义、数据质量和 schema version 丢失；列重命名可能破坏下游脚本。

### 10.2 RecordSink 写入结果不可见

open 返回 bool，writeHeader/writeRow/close 返回 void。磁盘写满、序列化失败或 close 失败无法进入统一运行结果。

### 10.3 Summary 是自由文本

`ISummaryObserver` 写 ostream，SimulationSummary 生成文本文件。机器可读指标、阈值、证据引用和报告模板必须再次解析或重新计算。

### 10.4 Run Manifest 不完整

effective mission 是重要复现入口。系统还需要稳定记录：源配置及展开来源、Execution Plan 哈希、模型包与资产哈希、代码版本、编译器和平台、数值策略、随机种子、开始/结束状态、诊断和产物清单。

### 10.5 Artifact 谱系尚未成为一等模型

CSV、summary、effective mission 和外部工具文件目前主要靠目录约定联系。

**影响**：缓存复用、增量重算、图表溯源、报告审计和跨实验比较都需要脚本自行猜测。

## 11. 研究工作流与外部工具审阅

当前框架能够运行 mission 和 SimFlow case，但“从研究问题到论证报告”的中间能力尚未形成架构层。DATCOM、配平、控制分析、GPOPS2、MATLAB、Origin、Python、Word 和 Excel 一旦逐项接入，若只增加脚本入口，会快速形成第二套隐式框架。

主要缺口：

- 任务输入输出 schema；
- 工具版本、许可证和运行环境记录；
- 工作目录与命令安全；
- 超时、取消、重试和失败分类；
- Artifact 缓存与内容寻址；
- 人工审核门；
- 数据单位、坐标和约定的适配声明；
- 模板版本和报告证据引用。

目标工作流引擎应建立在 Artifact 契约上，并与仿真内核保持进程和时间语义隔离。

## 12. 多语言、LLM 和前端审阅

### 12.1 Python/RL

当前 runner 和 Simulator 以单次命令行为主要入口。训练环境需要多个独立 Session、快速 reset、step 级控制、内存观测、动作校验、随机种子隔离和无全局 cwd 假设。

直接用 pybind 暴露内部类会把裸指针、生命周期和 Eigen 细节带到 Python，阻碍后续兼容。因此应先稳定应用控制接口和 schema，再提供语言绑定。

### 12.2 LLM

LLM 若直接编辑 JSON 并启动仿真，会放大默认值、版本、权限和复现风险。它需要查询 catalog、生成结构化提案、调用编译器、解释诊断、展示 diff 与假设、通过策略门、执行并记录审计。

### 12.3 蓝图式编辑器

当前真实依赖边未持久化，端口也没有稳定 id。界面只能从 placement 和接口名推测连线。Mission IR 与 PortDescriptor 是建设蓝图编辑器的前置条件。

### 12.4 实时与游戏前端

实时渲染循环、输入采样和仿真推进具有不同 clock。前端若直接读写节点状态，会破坏确定性和线程安全。未来需要命令队列、不可变发布快照、时间同步、丢帧策略和权威状态边界。

## 13. 构建、包和仓库结构审阅

### 13.1 header-only 的收益与代价

header-only 降低了初期链接和分发复杂度，也让模板注册容易实现。随着核心文件增长，它会放大编译时间、实现泄漏、ABI 无边界和全量重编译。

目标无需立即转成大型二进制 SDK。可先把稳定接口、纯实现和适配器在逻辑上分层，再用编译时间和语言绑定需求决定物理拆分。

### 13.2 项目组件发现依赖源码扫描

CMake 通过宏文本发现组件头。该方式适合当前规模，元数据提取和多项目复用能力有限。目标分支改用 typed static C++ descriptor 作为唯一来源，package contribution 显式列出 descriptor，再由确定性 exporter 生成离线 Catalog、schema 和文档；注册宏与源码扫描随旧路径删除。

### 13.3 中央 builtin bootstrap 容易增长

集中注册保证启动确定性，但随着领域包增多会形成巨大汇总点。目标分支直接改为各 package 提供 catalog contribution，再由 FrameworkCatalog 组合。

## 14. SOLID 视角下的具体判断

| 原则 | 当前表现 | 主要风险 | 目标方向 |
| --- | --- | --- | --- |
| SRP | 节点角色较清楚；Simulator、ConfigManager、AutoDataLogger 较宽 | 修改原因过多 | 拆分编译、Session、输出、manifest、adapter |
| OCP | 新节点可注册；新 schema、端口、sink 常需改中央代码 | 扩展点不完整 | descriptor、policy、adapter、package contribution |
| LSP | C++ 接口可替换；领域语义兼容性未被类型证明 | 同接口错语义 | 语义化 contract id、frame/unit/sample compatibility |
| ISP | 多数领域接口较小；接口层级混杂 | 稳定性和依赖难判 | 七类接口分层，按消费者最小化能力 |
| DIP | 组件通过接口绑定；Simulator 依赖具体日志和总结 | 内核依赖基础设施 | 端口、Session services、sink contracts |

这张表还低估了组件内部的 SRP 问题。代表性节点同时承担 config decode、依赖选择、算法、状态推进、输出缓存、观测枚举和注册；即使 placement 正确，类内部仍有七类变化原因。详见 18 节与 [12](12-runtime-object-model-and-component-anatomy.md)。

## 15. 技术债务分级

### A 级：会污染研究结论或阻塞后续架构

| ID | 问题 | 优先原因 |
| --- | --- | --- |
| A-01 | 单位、坐标和方向未进入稳定领域契约 | 可产生静默物理错误 |
| A-02 | 宽松配置读取与陈旧 config 风险 | 可运行错误实验 |
| A-03 | 依赖边未持久化 | 无法完成闭环和蓝图分析 |
| A-04 | 失败模型和 RunOutcome 缺失 | 无法区分有效终止与无效结果 |
| A-05 | 数值失败策略不一致 | 可能把失败伪装成合法数值 |
| A-06 | run 缺少异常安全收尾 | 产物和 Session 状态不可信 |
| A-07 | 离散组件原地改写状态，失败时无法整步回滚 | 可能形成离散/连续混合 epoch |
| A-08 | query 可通过 mutable cache 改变观测 | 积分器调用次数可能污染证据 |

### B 级：持续增加耦合和维护成本

| ID | 问题 |
| --- | --- |
| B-01 | Simulator 同时负责编排、内核与 I/O |
| B-02 | NodeFactory 元数据无法驱动工具 |
| B-03 | Mission 缺少 IR、版本和 source map |
| B-04 | Observable/RecordSink/Summary 契约过薄 |
| B-05 | 数学类型、积分器和控制状态模型重叠 |
| B-06 | 中央注册、header-only 和 active project 构建耦合 |
| B-07 | 组件壳与算法没有分层，结构代码重复 |
| B-08 | phase 内依赖用 priority/注册顺序表达 |
| B-09 | mode、phase、物理 configuration ownership 模糊 |

### C 级：远期场景所需能力

| ID | 问题 |
| --- | --- |
| C-01 | Workflow/Artifact 引擎缺失 |
| C-02 | Python Session API 缺失 |
| C-03 | LLM 策略与审计环缺失 |
| C-04 | 蓝图端口和布局模型缺失 |
| C-05 | 实时命令/快照边界缺失 |
| C-06 | 动态 package 与跨进程协议尚未验证 |

## 16. 保留、重构与退出清单

### 16.1 保留并强化

- placement 职责地图；
- configure/bind/prepare/initialize 生命周期；
- 周期开始发布态语义；
- 固定 phase 顺序；
- 同步独立积分与显式 continuous group；
- SimFlow 外置原则；
- project-first 晋升路径；
- 严格 builtin 配置和路径化诊断意图。

### 16.2 在目标分支直接替换

- NodeFactory 由 ComponentCatalog/package contribution 直接替换；
- AssemblyContext 名称绑定由 `BindingPlan` 和 compiled handles 直接替换；
- Mission JSON 装配链由 Mission Source + Mission IR + Execution Plan 直接替换；
- Simulator 由 SimulationSession、`CommittedStateStore`、StepTransaction 和专用 collaborators 直接替换；
- AutoDataLogger 由 ObservationPlan + RecordPipeline 替换；
- 文本 summary 改为 Structured Metrics + Report Adapter；
- math 结果族收敛为 NumericalOutcome；
- SimFlow 批量身份与调度收敛到 Experiment compiler/executor。

当前没有外部用户和历史运行兼容责任。重构保留科学参考、输入假设和可复现实验，源码 API、旧 Mission schema、节点结构、CSV 列表和偶然调度行为不设兼容目标。

### 16.3 从目标核心路径删除

- 旧 `math_types.hpp` 公共使用；
- 未知字段或错误类型的默认值回退；
- `getStateValue` 未找到时返回零；
- 物理关键插值的无标记 Clamp；
- 用 RTTI 文本作为稳定接口身份；
- 依赖边只保存在裸指针；
- 输出写失败只记录日志；
- summary 只生成自由文本；
- 前端直接持有运行节点指针。

## 17. 根因到目标分册的映射

| 根因 | 解决分册 |
| --- | --- |
| 缺少需求变化坐标、权威域和扩展闭包 | [02 系统架构蓝图与演进接缝](02-layered-reference-architecture.md) |
| 数学与物理语义重叠 | [03 数学与数值基础](03-mathematics-and-numerical-foundation.md) |
| 接口无分层和版本 | [04 领域契约与接口层](04-domain-contracts-and-interface-layer.md) |
| Mission 只是对象创建源 | [05 组件目录与 Mission 编译器](05-component-catalog-and-mission-compiler.md) |
| 运行期职责与失败状态混杂 | [06 仿真内核](06-simulation-kernel-time-and-lifecycle.md)、[07 诊断可靠性](07-diagnostics-reliability-and-observability.md) |
| 数据和报告缺少谱系 | [08 数据与研究证据](08-data-artifacts-and-research-evidence.md) |
| 外部工具会形成脚本孤岛 | [09 研究工作流](09-research-workflows-and-tool-adapters.md) |
| 多语言和前端可能绕过内核 | [10 扩展与前端](10-packages-multilanguage-and-frontends.md) |
| 组件、算法、状态和资产混放 | [12 运行对象与组件内部构成](12-runtime-object-model-and-component-anatomy.md) |
| 组件内部行为膨胀，mode、phase 和物理构型权威模糊 | [13 行为组合、嵌入机制与共享权威](13-behavior-composition-and-extension-mechanisms.md) |
| 同周期顺序、离散回滚与连续闭合隐含 | [14 周期数据流与状态事务](14-cycle-dataflow-state-transaction-and-continuous-closure.md) |
| 目标对象难以映射到真实项目文件 | [15 纵向参考设计](15-reference-vertical-designs-and-object-placement.md) |

## 18. 组件内部构成与数据流补充审阅

本轮进一步读取了组件内部行为工具、Simulator、6DoF form/interaction、YYZ 纵向组件、Mission priority 和 CAVH 复杂制导节点，得到下列事实。

### 18.1 组件内部行为缺少组合边界

当前 legacy callback state-machine helper 只保存 current enum 与 onEnter/onExit/onUpdate 回调，项目组件也没有实际使用它。YYZ sequencer 通过时间表和字符串自行计算 phase，propulsion 再比较 `enabled_phase_name`。Session 另有一套 `ExecutionPhaseManager`。

更深层问题是组件内部行为工具没有统一嵌入契约。状态机、滤波、滞回、抗饱和、故障锁存和交班逻辑的状态应归宿主 owner block；shared flight phase、导航源选择和 physical configuration 则需要窄 `DecisionAuthority` RuntimeCell；Session lifecycle 继续属于执行基座。当前实现没有表达这三类边界，回调式工具也无法自然进入 staged transaction、checkpoint 或 plan explain。

### 18.2 算法与组件壳高度耦合

代表性文件显示：

- YYZ guidance 在一个类中完成配置、绑定、坐标变换、LOS 计算、姿态误差、命令缓存和观测；
- actuator 将饱和、速率限制、差分状态和 provider 壳混合；
- aero 将资产路径、表装载、查询、扰动、当前 response 和观测混合；
- CAVH glide guidance 将优化、有限差分、两套论文公式、fallback、命令和大量 telemetry 成员混合。

`initialize()->update({0,0,0})` 在多个组件中出现，说明 initial-state construction 与 scheduled algorithm evaluation 没有独立契约。

### 18.3 复制聚合产生伪组件

`OnboardStateProcess` 只复制 Navigation、AirData 和 Phase，没有产生新估计或权威。它迫使 Mission 手工安排 priority，并让 guidance 依赖新的 provider。该职责应由 compiled InputBundleView 完成。

### 18.4 同 phase 数据新旧由 priority 决定

YYZ Output phase 用 priority 明确排列 actuator、propulsion、mass、aero。Mass 读取 Propulsion 当前成员，Aero 读取 Actuator 当前成员。Scheduler 没有使用 provider-consumer 边拓扑排序，注册顺序仍是 tie-break。

因此同一接口调用无法自行说明读取当前周期或上一周期，改 Mission priority 可能改变物理结果。

### 18.5 离散与连续提交边界分裂

Simulator 只对多个 continuous candidate 做统一 `setState()`；所有 discrete update 已经原地修改组件。后续 update 或 integration 失败时，continuous state 留在 x_k，部分 discrete state 已前进。

目标需要 committed-state-in / ComponentDelta-out，并把即时离散 patch、区间 candidate 和连续 candidate 纳入一个 StepTransaction。

### 18.6 closure 和 query 存在隐藏副作用

YYZ interaction 的 const query 写 `mutable last_input_`。Form publish 与 derivative 都会调用它，RK stage 数量会改变最后缓存。Aero/propulsion/mass response 又大多来自 t_k 离散成员，实际 closure 是隐含的 frozen interval。

目标必须区分 PureQuery、PublishedClosureSample 和 Integrator stage telemetry，并在 Execution Plan 选择 Frozen、Candidate 或 Algebraic closure。

### 18.7 领域 contract 的 cohesion 过宽

YYZ 单一 types header 同时定义所有数据与 provider interface。GuidanceCommand 用整数 mode 加多个互斥字段，Phase 使用 string/int，AeroState 又复制 AirData。该结构难以形成独立版本、最小 consumer input 和 tagged command。

以上问题的目标对象、行为组合、逐变量落位和纵向重构见 [12](12-runtime-object-model-and-component-anatomy.md)、[13](13-behavior-composition-and-extension-mechanisms.md)、[14](14-cycle-dataflow-state-transaction-and-continuous-closure.md) 与 [15](15-reference-vertical-designs-and-object-placement.md)。

## 19. 本轮审阅的边界

本册提供架构级事实判断，没有逐函数列出所有实现缺陷，也没有对每个气动、导航、制导和控制模型做物理正确性评审。后续落地应分别建立：

- 数学算法验证清单；
- 领域模型成熟度与验证档案；
- 接口兼容矩阵；
- Mission 编译器诊断样例库；
- Runtime 失败点注入测试；
- 端到端研究工作流黄金案例。
