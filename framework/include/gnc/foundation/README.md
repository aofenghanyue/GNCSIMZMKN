# foundation

职责：数学与数值存储、纯算法基础、值工具、稳定低层 identity helper。

允许依赖：C++ 标准库和经 ADR 批准的纯数值库。

禁止依赖：contracts 以上模块、配置格式、Session、日志、文件、具体 package。

当前 R1 切片：

- `numerical_outcome.hpp`：稳定 `NumericalStatus`、typed value/failure 与数值证据字段；
- `numerical_policy.hpp`：绝对加相对容差比较和 finite-check policy；
- `fixed_rk4.hpp`：可消费索引式向量状态的 classical fixed-step RK4，返回 typed outcome；
- `trilinear_table.hpp`：借用调用方持有的只读三轴表，准备阶段验证有限严格递增轴、网格形状和有限输出；查询阶段按 `x-major/y-middle/z-fastest` 布局执行闭区间严格域三线性插值，越界时返回无值 `OutOfRange`；
- `bracketed_root.hpp`：连续标量函数的闭区间二分求根；接受异号 bracket 或精确端点，按残差/自变量容差停止，并在 `NoBracket`、`MaxIterations` 与 `ToleranceUnreachable` 结果中保留最后 bracket 证据；
- `r1.foundation-numerics.oracle`：以 R0 minimal 3DoF 的 50 位解析 oracle 检查轨迹、四阶收敛、终止和 stage failure；
- `r1.foundation-trilinear.oracle`：以 R0 YYZ 气动查表的 80 位 Decimal oracle 检查内部点、端点、上边界、准备失败和严格越界失败；
- `r1.foundation-root.oracle`：把 R0 CAVH 抛物线阻力极值导数作为真实 consumer，与解析 `alpha_star_rad` 和 80 位 Decimal reference 比较，同时检查二分收敛、无括区间、迭代耗尽、非有限函数值及浮点容差不可达。

`PreparedTrilinearTableView` 不拥有轴或表格内存；调用方需保证其只读存储覆盖准备结果及全部查询的生命周期。当前求根器只覆盖具有连续性和显式 bracket 的标量问题，尚未提供导数、初值式局部求解或多变量求根。规范 Eigen storage、动态矩阵/分解、其他维度或策略的插值和自适应积分尚未进入当前切片。
