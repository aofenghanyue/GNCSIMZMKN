# foundation

职责：数学与数值存储、纯算法基础、值工具、稳定低层 identity helper。

允许依赖：C++ 标准库和经 ADR 批准的纯数值库。

禁止依赖：contracts 以上模块、配置格式、Session、日志、文件、具体 package。

当前 R1 切片：

- `numerical_outcome.hpp`：稳定 `NumericalStatus`、typed value/failure 与数值证据字段；
- `numerical_policy.hpp`：绝对加相对容差、零判据、条件数上限和 finite-check policy；
- `linear_algebra.hpp`：由 Eigen 3.4.0 提供的规范 `Vec3`、`Mat3`、`Vector`、`Matrix` 和 `QuaternionStorage` 存储别名；
- `passive_quaternion.hpp`：实现 ADR-0007 的被动 Hamilton 四元数路径，显式转换 `[w,x,y,z]`，提供真实逆元旋转、被动组合、旋转矩阵、body/inertial rate 导数、符号等价姿态误差，以及 `Error` / `NormalizeWithFlag` 输入策略；
- `fixed_rk4.hpp`：可消费索引式向量状态的 classical fixed-step RK4，返回 typed outcome；
- `trilinear_table.hpp`：借用调用方持有的只读三轴表，准备阶段验证有限严格递增轴、网格形状和有限输出；查询阶段按 `x-major/y-middle/z-fastest` 布局执行闭区间严格域三线性插值，越界时返回无值 `OutOfRange`；
- `bracketed_root.hpp`：连续标量函数的闭区间二分求根；接受异号 bracket 或精确端点，按残差/自变量容差停止，并在 `NoBracket`、`MaxIterations` 与 `ToleranceUnreachable` 结果中保留最后 bracket 证据；
- `local_newton_root.hpp`：消费函数值与解析导数的有限域局部 Newton 求根；不要求异号 bracket，显式报告初值、最后步长、局部收敛、导数退化、越域、迭代耗尽和浮点步长不可达；
- `spd_cholesky_3x3.hpp`：固定 `3×3` 对称正定 Cholesky 求解；显式验证 finite、对称、正定、近奇异和条件上限，并报告 rank、分解方法、无穷范数残差与 1-范数条件估计；
- `r1.foundation-numerics.oracle`：以 R0 minimal 3DoF 的 50 位解析 oracle 检查轨迹、四阶收敛、终止和 stage failure；
- `r1.foundation-trilinear.oracle`：以 R0 YYZ 气动查表的 80 位 Decimal oracle 检查内部点、端点、上边界、准备失败和严格越界失败；
- `r1.foundation-root.oracle`：把 R0 CAVH 抛物线阻力极值导数作为真实 consumer，与解析 `alpha_star_rad` 和 80 位 Decimal reference 比较，同时检查二分收敛、无括区间、迭代耗尽、非有限函数值及浮点容差不可达。
- `r1.foundation-local-root.oracle`：以同一 CAVH 极值导数和解析二阶导数验证局部 Newton；38 个初值样本直接形成两个 polar 的收敛域，代表轨迹检查渐近二次收敛，并覆盖 step tolerance、callback flag 传播和关键失败。
- `r1.foundation-spd-solve.oracle`：把 R0 YYZ 完整惯量张量和刚体转动方程作为真实 consumer，与 80 位 Decimal Cholesky reference 比较，同时检查缩放不变性、products-of-inertia mutation、近对称投影、奇异/非正定/病态和非有限输入。
- `r1.foundation-quaternion.oracle`：复用 R0 科学约定与 YYZ 6DoF oracle，以 80 位 Decimal 独立公式检查九条固定语义、耦合力旋转与姿态导数，并直接验证 256 组性质样本、principal-spin 五级 RK4 四阶收敛、归一化标志和关键失败。

`PreparedTrilinearTableView` 不拥有轴或表格内存；调用方需保证其只读存储覆盖准备结果及全部查询的生命周期。当前标量求根提供保守 bisection 与有限域 local Newton 两个明确入口；Newton 依赖调用方提供解析导数，不含 line search、自动 fallback、数值微分或多变量求解。当前矩阵求解只覆盖固定 `3×3` SPD 问题；动态矩阵分解、其他维度或策略的插值和自适应积分尚未进入当前切片。Foundation 四元数只承载数值存储和纯算法；frame/time 类型归属 R1 Contracts，Euler 转换仍要求调用方提供完整 profile metadata。
