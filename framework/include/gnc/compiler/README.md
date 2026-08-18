# compiler

职责：SourceTree/SourceMap、Catalog view、Canonical Model Graph、binding、proof、lowering、ExecutionPlanDescriptor 和 link contract。

允许依赖：foundation、contracts、model_sdk descriptors。

禁止依赖：具体 package 实现、Session state、Artifact format 和前端对象。

当前可运行切片：programmatic typed `SourceTree` 经 package-owned static descriptors 形成只读 Catalog、最小 Mission IR、exact-contract binding proof、compiled query/closure obligations 与静态 `ExecutionPlanDescriptor`。YYZ/CAVH package 只在 test composition root 组合，Compiler header 不引用具体 package。当前未包含 source parser、hash/serialization、`PlanProofIndex`、link image 或 runtime component。
