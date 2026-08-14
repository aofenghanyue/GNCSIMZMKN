# ADR-0006: SI, frame and simulation-time conventions

- Status: Accepted
- Date: 2026-08-10
- Owners: Scientific Authority, Architecture Lead
- Related tasks: R0-SCI-001
- Architecture references: 03 §6–§7、03 §9、06 §5

## Context

物理兼容需要同时约束数值、单位、frame、方向与时间基准。Legacy 接口广泛使用无标签 `double`、`Vector3` 和 `time`，相同表示可以承载位置、速度、力、采样时刻或墙钟时刻。新框架目前仍是 R0 skeleton，尚未建立产品级 Quantity、FrameGraph 或时间类型，因此需要先形成独立于实现的科学权威。

R0 允许隔离 validation spike，禁止提前建设产品级 Session、Compiler 和公共数学层。本 ADR 只冻结语义与 executable fixture，不选择生产存储类型或第三方单位库。

## Decision

运行时物理量统一采用 SI 基本/导出单位，角度采用弧度。非 SI、显示单位和 offset unit 只在 Config Adapter、Tool Adapter、Artifact Adapter 或显式 Domain Converter 边界转换；转换前后的值与 unit identity 必须可追踪。算法内部可以使用紧凑标量/向量，公共领域边界必须表达量纲与语义。

单位边界只接受有限数值和已登记单位；未知单位、转换溢出或低于绝对零度的温度产生 `DomainError`。`0 K` 与等价的 `-273.15 °C` 是合法边界值，不得用模糊钳位掩盖越界输入。

向量采用列向量。`R_to_from` 把同一几何对象从 `from` frame 坐标变换到 `to` frame 坐标，满足 `v_to = R_to_from * v_from`。稳定 `FrameId` 带 namespace/version，实例 frame 还带 owner。领域三维量必须区分 point/free vector、expressed-in frame、reference frame、physical semantic、unit、sample/valid time。point 变换应用旋转和平移，free vector 只应用旋转；速度、加速度、wrench 与 covariance 使用各自显式变换类别。

公共时间 identity 固定为 `SimulationTime`、`Duration`、`SampleTime`、`ValidTime` 与 `WallTime`。固定步长以非负整数 tick 为权威，通过 `t_k = time_origin + tick * base_dt` 计算逻辑时间，禁止重复浮点加法累积。v1 duration 对齐支持 `ExactGrid`、`StopBefore` 与 `StopAfter`，默认 `ExactGrid`；`FinalPartialStep` 在事务/time-point mapping 决策完成前保持 unsupported。

时间秒值必须有限；`SimulationTime`、`SampleTime` 与 `ValidTime` 必须携带非空 clock domain，同类时间点的算术只允许在相同 domain 内进行，跨 domain 操作产生 `DomainError`。R0 fixture 以半开区间 `[valid_from, valid_until)` 验证 validity 边界并拒绝反向、跨 domain 与非有限区间；该区间规则已随本 ADR 接受为科学基线，未来产品 API 仍需在对应实现切片中定义。

`fixtures/ref-scientific-conventions/conventions.json` 是本 ADR 决策的 executable profile，`cases.json` 固定方向、单位、point/free-vector 与 tick 示例。两个文件保持 Fixture maturity，不进入 runtime，也不构成语言绑定 ABI。

## Consequences

- Positive: unit、frame、direction 和 time 错接可以在模型绑定前被拒绝。
- Positive: 固定步长长时间运行不依赖重复浮点累加的历史误差。
- Positive: 外部工具仍可使用其原生单位和布局，边界映射保持可审计。
- Costs: public contract 和 artifact 需要携带额外 identity/validity metadata。
- Risks: R0 fixture 只证明冻结语义，尚未证明未来生产类型的性能与易用性。
- Modules kept unchanged: `framework/`、`packages/`、`adapters/`、`apps/`、`user/`。

## Alternatives considered

- 全部公共值继续使用无标签 `double/Vector3`：接口紧凑，但无法在编译或 bind 阶段证明物理兼容。
- 所有算法内部采用编译期单位模板：安全性高，热路径、矩阵运算和语言绑定成本尚未获得证据。
- 以浮点 SimulationTime 为唯一权威：实现直接，但固定步长会积累与执行历史相关的时间误差。
- 使用一个全局 frame：无法表达地固、惯性、本地、机体、风轴和实例 frame 的转换责任。

## Verification

- fixture 精确登记 SI/弧度单位表、允许转换边界与 offset conversion；
- 90° 被动旋转、point/free-vector 变换使用同一 `R_to_from` 并得到不同平移结果；
- 单位案例覆盖 degree/radian、km/h 到 m/s 与 Celsius 到 Kelvin；
- 单位边界拒绝未知单位、非有限数值、溢出与低于绝对零度，同时接受 `0 K`；
- 大 tick 时间由一次乘加表达，duration 非整网格案例分别验证 StopBefore/StopAfter，ExactGrid 拒绝非整数倍；
- 五类时间 identity 以隔离类型验证；clock-domain 算术拒绝空 domain、跨 domain 与非有限值，validity 验证半开端点及非法区间；
- C++17 property spike 与独立 CPython 标准库参考在逐量容差内一致；
- CTest 和 repository verification 检查 profile 与交叉工具执行结果。

## Supersession rule

改变规范单位、FrameId identity、point/free-vector 分类、五类时间 identity、tick 权威或 v1 duration policy 时必须提交 superseding ADR、迁移规则和双版本 fixture。把 executable profile 用于 runtime 或公开 API 时必须另行定义 schema、兼容性和 consumer evidence。
