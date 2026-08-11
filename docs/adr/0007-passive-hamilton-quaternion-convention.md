# ADR-0007: Passive Hamilton quaternion convention

- Status: Proposed
- Date: 2026-08-10
- Owners: Scientific Authority
- Related tasks: R0-SCI-001
- Architecture references: 03 §8、03 §22、roadmap/r0-r2-foundations §2.2

## Context

姿态实现即使数值稳定，也可能因主动/被动旋转、frame direction、乘法顺序或 coefficient order 不一致而得到物理错误。Legacy 同时存在自定义 `gnc::math::Quaternion`、轻量 `gnc::Quaterniond` 与直接 `Eigen::Quaterniond`。Eigen 的常用 vector action 是主动旋转，自定义类实现了共轭在左的被动变换；同名姿态字段因而不能按四个系数直接迁移。

目标蓝图 03 §8.1 已给出用户修订后的唯一权威规则。本 ADR 将规则收窄成可测试等式和序列化契约，不选择产品级 storage backend。

## Decision

四元数使用 Hamilton algebra，写作 `q_to_from = [w, x, y, z]`，与 `R_to_from` 表达同一被动坐标变换。纯四元数向量满足：

```text
v_to = inverse(q_to_from) * pure(v_from) * q_to_from
```

Hamilton product 的标量/向量形式固定为：

```text
(w1, u1) * (w2, u2)
= (w1*w2 - dot(u1,u2), w1*u2 + w2*u1 + cross(u1,u2))
```

矩阵与四元数组合分别满足：

```text
R_c_a = R_c_b * R_b_a
q_c_a = q_b_a * q_c_b
```

对正 z 轴的 `+90°` axis-angle 四元数 `[sqrt(1/2), 0, 0, sqrt(1/2)]`，本约定把 from-frame 的 `[1, 0, 0]` 坐标变换为 to-frame 的 `[0, -1, 0]`。该案例固定 axis-angle 符号和被动方向。

外部序列化顺序严格为 `[w, x, y, z]`。storage backend 的内存布局不得推断为 wire order；adapter 必须逐字段映射。输入必须恰有四个有限系数。单位四元数 `q` 与 `-q` 表达同一旋转。零模四元数产生 `DomainError`；非零非单位输入必须显式选择 `Error` 或 `NormalizeWithFlag`，后者正规化并把 `normalized` correction flag 置为真，禁止无记录的隐式修正。

Euler 表示没有隐式默认值。每个 Euler payload 同时声明 axis sequence、intrinsic/extrinsic、angle unit、canonical range 与 singular interval；跨奇异区和 ±π 的相等性通过旋转对象判断。oracle 只为验证固定一个带完整标签的 profile：intrinsic ZYX、分量顺序 yaw-pitch-roll、单位 rad、pitch 区间 `[-pi/2, pi/2]`。它满足 `q_I_B = inverse(q_z(yaw) * q_y(pitch) * q_x(roll))`，在 `abs(cos(pitch))` 不超过 policy tolerance 时拒绝无唯一解的反向转换；该 profile 不成为 runtime 默认值。

对于 `q_I_B` 和机体系表达的 `omega_BI_B`，由上述被动定义可导出 `q_dot_I_B = -0.5 * pure(omega_BI_B) * q_I_B`；若角速度以惯性系表达，则等价为 `q_dot_I_B = -0.5 * q_I_B * pure(omega_BI_I)`。该导数关系进入 oracle，不改变未来积分 policy。

## Consequences

- Positive: direction、composition、matrix equivalence 与 serialization 可由单一 oracle 判断。
- Positive: Eigen、自定义类型和外部工具必须通过声明式 adapter，避免系数相同但语义相反。
- Positive: `q`/`-q` 与 Euler wrap 不再造成伪差异。
- Costs: 字段和 API 名称必须携带 `to/from` direction，适配代码需要显式 conjugation/order mapping。
- Risks: property spike 是隔离参考实现，尚未决定 Eigen 或自研 storage 进入产品公共路径。
- Legacy disposition: 自定义 passive rotation 代数可作为对照；zero-to-identity 行为标记 Fix；无方向轻量类型标记 Retire；Eigen direct-use 需要显式 representation adapter。

## Alternatives considered

- 采用常见主动 `q * v * inverse(q)`：可直接匹配 Eigen vector action，但会推翻 03 §8.1 的用户权威规则和现有右乘组合约定。
- 序列化采用 `[x, y, z, w]`：可贴近部分库的 coefficient storage，但与蓝图和 Legacy 配置中的 scalar-first 数据冲突。
- 零模自动替换为 identity：可以继续运行，却会把无效姿态变成看似合法的物理状态。
- 允许各组件选择约定：减少短期 adapter 数量，但无法建立跨组件科学 oracle。

## Verification

- 单轴 90°/180° 案例固定方向与 axis-angle 符号；
- Hamilton coefficient、右乘 composition、inverse round-trip 与 matrix action 逐项验证；
- 随机单位四元数验证矩阵正交性、行列式 `+1`、`q/-q` 等价与 matrix/quaternion equivalence；
- `[w, x, y, z]` serialization 做 exact-order 检查，零模路径必须抛出 domain failure；
- 反序列化拒绝系数数量错误、`NaN`/无穷与零模；非单位输入分别验证 `Error` 拒绝和 `NormalizeWithFlag` 修正/标记；
- 带完整 metadata 的 intrinsic-ZYX profile 验证 quaternion/matrix/Euler 往返，并拒绝 pitch 奇异点；
- body/inertial angular-rate derivative 等式以有限差分/代数性质检查；
- C++17 spike 与独立 CPython 标准库实现对 executable fixture 交叉验证。

## Supersession rule

改变主动/被动语义、Hamilton algebra、`to/from` direction、composition order、axis-angle 符号、wire order、zero-norm disposition 或 derivative convention 时，必须提交 superseding ADR 和新的 convention id；旧 fixture 与 adapter 保留到所有 artifact consumer 完成迁移。
