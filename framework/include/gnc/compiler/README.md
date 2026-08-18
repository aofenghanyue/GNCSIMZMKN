# compiler

职责：SourceTree/SourceMap、Catalog view、Canonical Model Graph、binding、proof、lowering、ExecutionPlanDescriptor 和 link contract。

允许依赖：foundation、contracts、model_sdk descriptors。

禁止依赖：具体 package 实现、Session state、Artifact format 和前端对象。

当前可运行切片：programmatic `TypedStaticCompositionSource` 经 package-owned static descriptors 形成只读 Catalog、`StaticCompositionIr`、exact-contract binding proof、compiled query/closure obligations 与窄静态 `ExecutionPlanDescriptor`。端口只允许 model Output 到 algorithm Input，consumer input 为单值 required。YYZ/CAVH package 只在 test composition root 组合，Compiler header 不引用具体 package。当前没有 syntax-neutral `SourceTree`/`SourceMap`、canonical model config、asset binding、hash/serialization、`PlanProofIndex`、link image 或 runtime component；plan 不能重建完整 PreparedModel。
