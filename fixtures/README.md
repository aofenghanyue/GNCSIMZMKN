# Reference fixtures

Fixture 是可复制、可校验、带 provenance 的规范实例。每个目录至少包含 `fixture-manifest.json`，并在 `specification_only` 阶段登记计划验证的 expected facts 与 tolerance policy。达到 executable 后加入权威输入、机器可比 expected values、失败样例和运行脚本。

当前两个场景 fixture 保持 `specification_only`：

- `ref-yyz-001`：source→plan→session→observation→evidence 主链；
- `ref-cavh-formula`：复杂论文算法的公式级 reference。

`ref-scientific-conventions` 已达到 `executable`，冻结 SI、frame、整数 tick 与被动 Hamilton 四元数约定，并由隔离 C++17 property spike 和独立 Python 标准库实现交叉验证。

`ref-minimal-3dof` 已达到 `executable`，包含显式初值、高精度闭式轨迹、独立 C++17 RK4 probe、收敛检查、committed-tick 终止和 candidate 丢弃失败用例。

`ref-yyz-6dof-core` 已达到 `executable`，包含已接受的 fixture-local 惯性笛卡尔刚体方程、公式 intermediates、独立 Decimal/C++17 实现、解析平移与主轴自旋轨迹、非主轴无外力矩高精度轨迹、姿态与角速度四阶收敛、转动能和角动量模守恒检查、ExactGrid 终止、阶段失败及输入域拒绝。canonical mission、环境、气动、推进、制导控制、终止指标与生产容差仍在 `R0-SCI-003` 后续范围内。

`ref-yyz-force-moment-closure` 已达到 `executable`，包含已接受的 fixture-local `FrozenInterval` 体轴力/矩闭合、质心到作用点的力矩搬移、按 source identity 的规范化结果、独立 Decimal/C++17 实现、闭合到刚体核心的解析短轨迹、贡献顺序等价、五条输入域拒绝和三条物理 mutation。重力保持独立惯性加速度；canonical 子系统模型、可变质量属性和 candidate/algebraic closure 仍在后续范围内。

`ref-yyz-air-data-kinematics` 已达到 `executable`，包含已接受的 fixture-local 右手 `x-forward/y-right/z-down` 体轴、风速相减、被动 Hamilton 惯性系到体轴旋转、无 clamp 的 alpha/beta、动压与 Mach 公式。80 位 Decimal 与独立 C++17 实现交叉检查五个公式 case、两条等价性、九条输入域失败和四条 sign/direction/clamp mutation；canonical 环境、传感器、气动适用域和产品 contract 仍在后续范围内。

`ref-yyz-aero-dimensionalization` 已达到 `executable`，包含已接受的 fixture-local supplied coefficient 维度化：`[-C_A,+C_Y,-C_N]` 体轴力映射，roll/yaw 使用展长、pitch 使用参考弦长，以及从显式 aerodynamic reference point 到质心的力矩搬移。80 位 Decimal 与独立 C++17 实现交叉检查三个公式 case、两条等价性、十条输入域失败和三条 sign/scale/reference-vector mutation；coefficient lookup、插值、asset provenance、canonical 几何与产品 contract 仍在后续范围内。

`ref-yyz-uniform-environment` 已达到 `executable`，包含已接受的 fixture-local pure query：显式提供的惯性系重力、air-mass velocity、密度和声速对有限查询位置与非负 tick 保持物理恒定，并在同一 sample identity 直接供给 air-data 与 rigid-body 公式。80 位 Decimal 与独立 C++17 实现覆盖普通 consumer link、零密度/亚单位声速边界、position/tick 等价、严格输入域及 Legacy-style altitude decay 失败；Earth、海拔模型、风廓线、canonical constants 与产品 contract 仍在后续范围内。

`ref-yyz-propulsion-response` 已达到 `executable`，包含已接受的 fixture-local supplied propulsion response：非负推力标量与显式单位体轴方向组成力，响应携带作用点和该点固有力矩，Closure 独占 `r × F` 搬移；正值燃料消耗率在半开区间内积分并从 committed mass 扣除。80 位 Decimal 与独立 C++17 实现交叉检查三个公式 case、区间分割等价、十条输入域失败和三条 thrust/moment/mass mutation；command mapping、engine/fuel dynamics、dry-mass policy、canonical assets 与产品 contract 仍在后续范围内。

`ref-yyz-mass-properties` 已达到 `executable`，实现由既有 accepted invariants 固定的 projection-only 路径：同边界 committed MassState 投影正质量、CoM 点坐标和关于 CoM 的完整对称正定体轴惯量；CoM 与作用点坐标生成 Closure 杠杆臂，质量和完整惯量直接进入 rigid-core consumer。待提交质量候选在当前 `FrozenInterval` 内保持 candidate-only，下一边界仅从显式 next committed state 投影。燃料驱动的 CoM/惯量演化、dry mass、configuration jump、canonical assets 与产品 contract 仍在后续范围内。

`ref-yyz-frozen-interval` 已达到 `executable`，把已接受的 uniform environment、air-data、aero dimensionalization、propulsion response、当前 MassProperties、Closure 与刚体核心组合在同一 `[0,1)` 区间。80 位 Decimal 常加速度闭式轨迹与独立 C++17 一步 RK4 交叉检查 tick 1 状态、候选质量延迟可见性、四元数符号等价、八条输入域拒绝和三条跨组件 mutation。canonical assets、lookup、制导控制、产品终止指标与提交后的质量几何仍在后续范围内。

`ref-legacy-sync-commit` 已达到 `executable`，将 `ORACLE-YYZ-SYNC-03` 的冻结来源、独立 Decimal 结果、C++17 candidate/commit journal 和 early-commit 失败用例连成切片；已接受的处置保留 candidate barrier 与 committed-`t_k` 读取，并退出 Legacy 实现形状。

`ref-legacy-publish` 已达到 `executable`，交叉验证 publish 前后状态恒等、truth 边界时间、解析轨迹与 publish-time mutation 失败；已接受的处置保留只读发布与 `t_k` 边界刷新，并退出 Legacy 发布接口和存储形状。

`ref-legacy-phase` 已达到 `executable`，包含冻结 Legacy 外部 harness 的两份原始七阶段 trace、无 Legacy 依赖的独立 C++17 调度 probe，以及 phase swap 和 duplicate 失败用例；已接受的处置保留固定宏阶段顺序，并退出 priority、registration/config tie-break 与 Legacy callback 表面。

`ref-legacy-continuous-group` 已达到 `executable`，包含真实 Legacy 四阶段 joint-candidate trace、50 位 Decimal 与独立 C++17 RK4 reference，以及 split closure、未注册 member 和重复 scope ownership 失败用例；成员按稳定 identity 绑定，声明与 packed storage 重排保持语义等价。已接受的处置保留共享 candidate、单次 scope commit 和唯一 membership，并退出 Legacy group surface 与手工 vector packing。

`ref-legacy-csv` 已达到 `executable`，包含两份真实 Legacy CSV capture、fixture-local 语义字段映射、50 位 Decimal 与独立 C++17 reference；按表头映射后允许列置换、有限 Decimal 等值文本、未映射列变化及重复未映射表头，并拒绝缺失 t0、缺失必要列、错位 `t_k`、陈旧发布态、非有限必要字段值和重复必要表头。已接受的处置保留 `t_k` 与 published-state 边界，退出 Legacy 编码和路径形状。

`ref-legacy-stop` 已达到 `executable`，包含两份真实 t0 dataset、两份 record/termination trace、独立 Python/Decimal reference 与 C++17 timeline probe；终止判定会在返回 `true` 前读回已 flush 的停止状态，两项 record-field event 与 dataset 列按稳定 identity/header 映射后允许换序，并分别拒绝提前终止、评估时行不可见、缺失终止行、终态时间推进和停止后额外观测。已接受的处置保留停止状态 terminal `Observation` 先于对应 `RunOutcome` 的时间语义，退出 Legacy reason 文本、顺序规则与 runtime/CSV surface。

`ref-legacy-simflow` 已达到 `executable`，包含固定 base mission、两行 variation matrix、两次真实 `--simflow` 捕获和两次普通 `--config` 重放；普通重放使用 case 目录外的任务副本并从独立全新目录启动，四份 dataset 与两份 effective mission 分别保持 byte-identical，独立 Python/C++17 reference 验证输入注入、effective-mission JSON 重编码、requested-input、case-source 与 dataset 列重排、普通任务重放、目录、case-manifest absence 与 CLI spelling 身份无关性和五条直接失败路径。已接受的处置保留预运行自包含任务物化与 ordinary compile/run replay，退出 Legacy case identity 和 runtime surface。

禁止把蓝图中的演示数值直接升级为 golden。Scientific Authority 需要确认来源、公式和容差。
