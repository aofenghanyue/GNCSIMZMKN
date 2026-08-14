# Reference fixtures

Fixture 是可复制、可校验、带 provenance 的规范实例。每个目录至少包含 `fixture-manifest.json`，并在 `specification_only` 阶段登记计划验证的 expected facts 与 tolerance policy。达到 executable 后加入权威输入、机器可比 expected values、失败样例和运行脚本。

当前两个场景 fixture 保持 `specification_only`：

- `ref-yyz-001`：source→plan→session→observation→evidence 主链；
- `ref-cavh-formula`：复杂论文算法的公式级 reference。

`ref-scientific-conventions` 已达到 `executable`，冻结 SI、frame、整数 tick 与被动 Hamilton 四元数约定，并由隔离 C++17 property spike 和独立 Python 标准库实现交叉验证。

`ref-minimal-3dof` 已达到 `executable`，包含显式初值、高精度闭式轨迹、独立 C++17 RK4 probe、收敛检查、committed-tick 终止和 candidate 丢弃失败用例。

`ref-yyz-6dof-core` 已达到 `executable`，包含已接受的 fixture-local 惯性笛卡尔刚体方程、公式 intermediates、独立 Decimal/C++17 实现、解析平移与主轴自旋轨迹、非主轴无外力矩高精度轨迹、姿态与角速度四阶收敛、转动能和角动量模守恒检查、ExactGrid 终止、阶段失败及输入域拒绝。canonical mission、环境、气动、推进、制导控制、终止指标与生产容差仍在 `R0-SCI-003` 后续范围内。

`ref-legacy-sync-commit` 已达到 `executable`，将 `ORACLE-YYZ-SYNC-03` 的冻结来源、独立 Decimal 结果、C++17 candidate/commit journal 和 early-commit 失败用例连成切片；已接受的处置保留 candidate barrier 与 committed-`t_k` 读取，并退出 Legacy 实现形状。

`ref-legacy-publish` 已达到 `executable`，交叉验证 publish 前后状态恒等、truth 边界时间、解析轨迹与 publish-time mutation 失败；已接受的处置保留只读发布与 `t_k` 边界刷新，并退出 Legacy 发布接口和存储形状。

`ref-legacy-phase` 已达到 `executable`，包含冻结 Legacy 外部 harness 的两份原始七阶段 trace、无 Legacy 依赖的独立 C++17 调度 probe，以及 phase swap 和 duplicate 失败用例；已接受的处置保留固定宏阶段顺序，并退出 priority、registration/config tie-break 与 Legacy callback 表面。

`ref-legacy-continuous-group` 已达到 `executable`，包含真实 Legacy 四阶段 joint-candidate trace、50 位 Decimal 与独立 C++17 RK4 reference，以及 split closure、未注册 member 和重复 scope ownership 失败用例；成员按稳定 identity 绑定，声明与 packed storage 重排保持语义等价。已接受的处置保留共享 candidate、单次 scope commit 和唯一 membership，并退出 Legacy group surface 与手工 vector packing。

`ref-legacy-csv` 已达到 `executable`，包含两份真实 Legacy CSV capture、fixture-local 语义字段映射、50 位 Decimal 与独立 C++17 reference；按表头映射后允许列置换、有限 Decimal 等值文本、未映射列变化及重复未映射表头，并拒绝缺失 t0、缺失必要列、错位 `t_k`、陈旧发布态、非有限必要字段值和重复必要表头。已接受的处置保留 `t_k` 与 published-state 边界，退出 Legacy 编码和路径形状。

`ref-legacy-stop` 已达到 `executable`，包含两份真实 t0 dataset、两份 record/termination trace、独立 Python/Decimal reference 与 C++17 timeline probe；终止判定会在返回 `true` 前读回已 flush 的停止状态，两项 record-field event 与 dataset 列按稳定 identity/header 映射后允许换序，并分别拒绝提前终止、评估时行不可见、缺失终止行、终态时间推进和停止后额外观测。已接受的处置保留停止状态 terminal `Observation` 先于对应 `RunOutcome` 的时间语义，退出 Legacy reason 文本、顺序规则与 runtime/CSV surface。

`ref-legacy-simflow` 已达到 `executable`，包含固定 base mission、两行 variation matrix、两次真实 `--simflow` 捕获和两次普通 `--config` 重放；普通重放使用 case 目录外的任务副本并从独立全新目录启动，四份 dataset 与两份 effective mission 分别保持 byte-identical，独立 Python/C++17 reference 验证输入注入、effective-mission JSON 重编码、requested-input、case-source 与 dataset 列重排、普通任务重放、目录、case-manifest absence 与 CLI spelling 身份无关性和五条直接失败路径。已接受的处置保留预运行自包含任务物化与 ordinary compile/run replay，退出 Legacy case identity 和 runtime surface。

禁止把蓝图中的演示数值直接升级为 golden。Scientific Authority 需要确认来源、公式和容差。
