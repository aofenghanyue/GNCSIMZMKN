# R0-SCI-001 current scientific implementation inventory

日期：2026-08-10。范围只包含新框架 skeleton 与冻结 Legacy 的只读证据；本清单不改变 Legacy disposition，也不把旧实现引入新运行路径。

## 新框架状态

新框架 `framework/include/gnc/` 目前只有模块 README 与 `foundation/version.hpp`。没有公共 Vector、Matrix、Quaternion、Quantity、FrameId、SimulationTime 或 NumericalPolicy 实现。`tests/skeleton_smoke.cpp` 只验证 bootstrap metadata。

结论：R0-SCI-001 可以先冻结科学语义和 oracle；产品类型仍属于 R1 及后续纵向 slice。

本阶段新增的 `Duration`、`SimulationTime`、`SampleTime`、`ValidTime`、`WallTime`、四元数输入 policy 与单位转换器都只存在于隔离 C++/Python oracle 中，用于证明边界性质；它们不是产品 header、runtime consumer 或已选定的 storage/API 设计。半开 validity 行为同样只是 Proposed ADR 下的 fixture 候选，等待 Scientific Authority 决策。

## 冻结 Legacy 表示

| 表示/使用点 | 已观察语义 | 与 SCI-CONVENTIONS-001 的关系 | disposition |
| --- | --- | --- | --- |
| `framework/include/gnc/common/math/quaternion.hpp` | scalar-first Hamilton product；`q* v q` 被动 action；矩阵满足 reversed composition | direction、algebra 和 wire helper 可作为对照 | Preserve as evidence only |
| 同文件 `normalized()` | 小模长返回 identity | 目标要求零模产生 `DomainError` | Fix during migration |
| `framework/include/gnc/common/math_types.hpp` 的 `gnc::Quaterniond` | scalar-first 数据和宽松 normalize；无 frame/direction/action contract | 无法证明物理兼容 | Retire |
| form/project 的 `Eigen::Quaterniond` | 字段常命名 `attitude_body_to_*`，使用 Eigen 主动 vector/matrix convention | 与目标被动 `q_to_from` 需要 conjugation/direction adapter | Adapt explicitly |
| 6DoF form 的 zero-norm normalize | 零模替换为 identity | 会隐藏非法姿态 | Fix during migration |
| YYZ 6DoF 的 `q_dot = 0.5 * Q * omega_body` | 与主动 `Q` 表示一致 | 目标被动 `q = inverse(Q)` 使用负号和反向乘序；不得逐系数复用 | Adapt explicitly |
| 多处 `Vector3`/`double time` | unit、frame、point/free-vector、sample/valid time 主要依赖字段名 | 缺少可执行 metadata | Replace by domain contracts |

## 迁移约束

1. 不把 Legacy quaternion header、Eigen 类型或转换 helper 链接到新产品 target。
2. adapter 必须声明源表示的 active/passive action、from/to direction、coefficient order 与 normalization policy。
3. Eigen constructor 的 `(w,x,y,z)` 参数顺序不能证明其内部 coefficient storage 或 artifact wire order；序列化逐字段读取 `w/x/y/z`。
4. zero-to-identity 不能作为兼容 fallback。历史结果如依赖该行为，需要进入 old-behavior difference review。
5. 领域向量迁移必须同时补齐 semantic、unit、expressed-in/reference frame 与 sample/valid time，禁止只替换底层 `Vector3` typedef。

## 当前判定

本阶段没有可晋升的产品数学实现。可晋升对象只有 Proposed ADR、executable fixture、隔离 property test、独立 Python reference 与 cross-tool evidence。当前 oracle 已覆盖四元数有限/非单位输入、单位 domain failure、clock-domain 算术与 validity 边界，但这些测试类型不能被复制为产品 API。Scientific Authority 接受 ADR 后，后续 R1/N2/N3 才能选择 storage backend 和公共类型。
