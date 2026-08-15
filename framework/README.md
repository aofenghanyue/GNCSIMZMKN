# Framework

`framework/include/gnc/` 承载产品架构分区。当前 R1 Foundation 已提供 typed numerical outcome/policy、固定步长 RK4、严格域三线性插值、括区间与局部 Newton 求根、尺度化中心差分、显式前向/后向三点二阶单边差分、固定 `3×3` SPD 求解及被动 Hamilton 四元数纯算法。已交付范围以对应 probe 和独立高精度 oracle 为准。

首版采用 header-oriented 组织以缩短启动周期。编译库、生成代码或稳定 ABI 需要性能、构建或语言绑定证据，并通过 ADR 引入。

单边微分要求调用方给出有限域和方向，按实际 binary64 采样间距计算二次插值导数。越界、不可表示步长、回调失败与非有限中间量返回结构化 `NumericalOutcome`；算法不会自动换向、缩步或夹紧。
