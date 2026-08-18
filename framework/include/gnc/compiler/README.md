# compiler

职责：SourceTree/SourceMap、Catalog view、Canonical Model Graph、binding、proof、lowering、ExecutionPlanDescriptor 和 link contract。

允许依赖：foundation、contracts、model_sdk descriptors。

禁止依赖：具体 package 实现、Session state、Artifact format 和前端对象。

当前可运行切片：programmatic `TypedStaticCompositionSource` 经 package-owned static descriptors 形成只读 Catalog 与可独立构建的 `CanonicalMissionIr`，再生成 exact-contract binding proof、compiled query/closure obligations 与窄静态 `ExecutionPlanDescriptor`。REF-YYZ-001 source 已把 mission/entity/initial lifecycle、`Vehicle + subject_entity_id` scope、aero/closure subject、`vehicle.output | interaction/closure` placement、canonical config 和真实 aero asset binding 纳入 IR；CAVH GlideEnvelope 同时提供第二个 package-owned `vehicle.output` consumer，且不创建虚构 entity/scope。配置 schema 只使用 string、int64、enum 与 canonical binary64，package builder 可以从同一 block 确定性重建 GlideEnvelope、AerodynamicTable 和 ForceMomentClosure definition。

`hash_canonical_mission_ir` 对规范化 graph 生成 tagged、length-prefixed、big-endian semantic bytes 与 SHA-256；source location、声明顺序和 plan id 被排除。独立 Python reference 与 C++ vector 一致，非 canonical order、非有限值和负零会在 hash 前失败。未知 scope entity、occurrence unknown scope、subject/scope 不一致、source/package placement 冲突、config schema/type 差异与 asset role/schema/identity 错误均返回带 source ref 的直接诊断。Compiler production header 不引用具体 package，CAVH formula 与 YYZ rigid-step 仍处于 algorithm consumer 分区。

当前没有 syntax-neutral `SourceTree`/`SourceMap`、完整 `PlanProofIndex`、link image、持久化 serialization、RuntimeComponent 或 Session。plan 仍不能重建完整 PreparedModel。
