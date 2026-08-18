# compiler

职责：SourceTree/SourceMap、Catalog view、Canonical Model Graph、binding、proof、lowering、ExecutionPlanDescriptor 和 link contract。

允许依赖：foundation、contracts、model_sdk descriptors。

禁止依赖：具体 package 实现、Session state、Artifact format 和前端对象。

当前可运行切片：programmatic `TypedStaticCompositionSource` 经 package-owned static descriptors 形成只读 Catalog 和可独立构建的 `CanonicalMissionIr` entity/subject/identity/binding 子集，再由该 IR 生成 exact-contract binding proof、compiled query/closure obligations 与窄静态 `ExecutionPlanDescriptor`。独立 REF-YYZ-001 source 已把 mission/source identity、`vehicle.fixture.yyz@1`、`active_at_initialize` 及 ForceMomentClosure occurrence 的 subject relation 纳入 IR，并保留每项直接 source ref；空或重复 entity identity 与 unresolved subject 会在 IR 构建阶段失败。既有 YYZ+CAVH 双 package source 继续验证 identity/binding 与 plan lowering，其中 CAVH formula 和 YYZ rigid-step 只属于 algorithm consumer 分区。确定性 explain 排除 source location 与 plan identity；全空 source 在 IR 入口直接拒绝，部分 source 继续进入正常解析或 binding 诊断。端口只允许 model Output 到 algorithm Input，consumer input 为单值 required。YYZ/CAVH package 只在 test composition root 组合，Compiler header 不引用具体 package。当前没有 syntax-neutral `SourceTree`/`SourceMap`、scope/placement、canonical model config、asset binding、canonical semantic hash、持久化 serialization、`PlanProofIndex`、link image 或 runtime component；plan 不能重建完整 PreparedModel。
