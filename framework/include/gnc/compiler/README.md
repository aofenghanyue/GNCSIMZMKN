# compiler

职责：SourceTree/SourceMap、Catalog view、Canonical Model Graph、binding、proof、lowering、ExecutionPlanDescriptor 和 link contract。

允许依赖：foundation、contracts、model_sdk descriptors。

禁止依赖：具体 package 实现、Session state、Artifact format 和前端对象。

当前可运行切片：programmatic `TypedStaticCompositionSource` 经 package-owned static descriptors 形成只读 Catalog 和可独立构建的 `CanonicalMissionIr` identity/binding 子集，再由该 IR 生成 exact-contract binding proof、compiled query/closure obligations 与窄静态 `ExecutionPlanDescriptor`。IR 规范化 occurrence、精确 package/model/algorithm/preparation identity、端口 contract 和 binding intent；其确定性 explain 排除 source location 与 plan identity，同时保留 source ref 供后续诊断使用。全空 source 在 IR 入口直接拒绝，部分 source 继续进入正常解析或 binding 诊断。端口只允许 model Output 到 algorithm Input，consumer input 为单值 required。YYZ/CAVH package 只在 test composition root 组合，Compiler header 不引用具体 package。当前没有 syntax-neutral `SourceTree`/`SourceMap`、entity/scope、canonical model config、asset binding、canonical semantic hash、持久化 serialization、`PlanProofIndex`、link image 或 runtime component；plan 不能重建完整 PreparedModel。
