# model_sdk

职责：ModelDefinition、Algorithm six-piece、RuntimeCellRecipe、State/Port/Telemetry schema、execution obligations 和 behavior composition。

当前已交付的 descriptor 覆盖真实 `PureQuery`、`Closure`、首个 stateless `RuntimeComponent` 与 AlgorithmKernel consumer：package 提供 static model/algorithm/port/preparation 描述，R2 Compiler 以只读方式消费。`RuntimeComponent` 是封闭 execution-form tag，只有该 form 可以携带 recipe、profile、obligation、sampled ports、schedule、lifecycle 与 algorithm-entry facts；当前无状态 consumer 不开放 state schema，PureQuery/Closure 继续只有 prepare-time facts与 output ports。

允许依赖：foundation、contracts。

禁止依赖：Compiler、Kernel、Session、Artifact Store、adapter 和具体项目。

当前已交付的双 consumer 最小能力：

- `model_metadata.hpp`：immutable definition/prepared metadata、封闭 execution form 和 typed prepare failure；现有 PreparedModel 产品使用 PureQuery/Closure，RuntimeComponent tag 由静态 Catalog descriptor 消费。
- `algorithm_evaluation.hpp`：call-local formal output 与 telemetry 分离；YYZ/CAVH 的无状态 kernel 均直接返回该类型。
- `static_descriptor.hpp`：YYZ `AltitudePitchGuidance` 形成首个 stateless `SampledTransform` consumer；descriptor 不携带 state schema，schedule 为 process phase 每个 committed boundary 执行，lifecycle 只声明 instantiate/dispose。

RuntimeComponent plan、cell factory、PreparedModel cache、registry、serializer 与 StateFragment 仍未实现。R1 的当前独立求值路径不依赖 Mission、Session、文件系统或 logger。
