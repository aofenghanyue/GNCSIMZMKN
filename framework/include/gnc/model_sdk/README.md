# model_sdk

职责：ModelDefinition、Algorithm six-piece、RuntimeCellRecipe、State/Port/Telemetry schema、execution obligations 和 behavior composition。

当前已交付的 descriptor 仅覆盖真实 `PureQuery`、`Closure` 与 stateless AlgorithmKernel consumer：package 提供 static model/algorithm/port/preparation 描述，R2 Compiler 以只读方式消费。当前端口只表达 required model Output 与 required algorithm Input；descriptor 不冻结未经 Catalog 验证的 composition model identity。`RuntimeComponent`、recipe、profile、state 与 lifecycle descriptor 等待满足真实 ownership、调度、命令或 lifecycle 条件的模型选择。

允许依赖：foundation、contracts。

禁止依赖：Compiler、Kernel、Session、Artifact Store、adapter 和具体项目。

当前已交付的双 consumer 最小能力：

- `model_metadata.hpp`：immutable definition/prepared metadata、封闭 execution form 和 typed prepare failure；YYZ 使用 Closure，CAVH 使用 PureQuery。
- `algorithm_evaluation.hpp`：call-local formal output 与 telemetry 分离；YYZ/CAVH 的无状态 kernel 均直接返回该类型。

PreparedModel cache、registry/factory、serializer、StateFragment、RuntimeCellRecipe 和 execution obligation descriptor 等待对应真实 consumer。R1 的当前独立求值路径不依赖 Mission、Session、文件系统或 logger。
