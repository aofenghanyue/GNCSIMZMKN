# model_sdk

职责：ModelDefinition、Algorithm six-piece、RuntimeCellRecipe、State/Port/Telemetry schema、execution obligations 和 behavior composition。

允许依赖：foundation、contracts。

禁止依赖：Compiler、Kernel、Session、Artifact Store、adapter 和具体项目。

当前已交付的双 consumer 最小能力：

- `model_metadata.hpp`：immutable definition/prepared metadata、封闭 execution form 和 typed prepare failure；YYZ 使用 Closure，CAVH 使用 PureQuery。
- `algorithm_evaluation.hpp`：call-local formal output 与 telemetry 分离；YYZ/CAVH 的无状态 kernel 均直接返回该类型。

PreparedModel cache、registry/factory、serializer、StateFragment、RuntimeCellRecipe 和 execution obligation descriptor 等待对应真实 consumer。R1 的当前独立求值路径不依赖 Mission、Session、文件系统或 logger。
