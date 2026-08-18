# compiler

职责：SourceTree/SourceMap、Catalog view、Canonical Model Graph、binding、proof、lowering、ExecutionPlanDescriptor 和 link contract。

允许依赖：foundation、contracts、model_sdk descriptors。

禁止依赖：具体 package 实现、Session state、Artifact format 和前端对象。

当前可运行切片：programmatic `TypedStaticCompositionSource` 经 package-owned static descriptors 形成只读 Catalog 与可独立构建的 `CanonicalMissionIr`，再生成 typed `BindingPlan`、`TemporalBindingPlan`、结构化 proof、compiled query/closure obligations 与窄静态 `ExecutionPlanDescriptor`。REF-YYZ-001 source 已把 mission/entity/initial lifecycle、`Vehicle + subject_entity_id` scope、aero/closure subject、scoped RigidStep algorithm consumer、`vehicle.output | interaction/closure` placement、canonical config、真实 aero asset binding 和两条 binding intent 纳入 IR；CAVH GlideEnvelope 同时提供 package-owned `vehicle.output` consumer，且不创建虚构 entity/scope。配置 schema 只使用 string、int64、enum 与 canonical binary64，package builder 可以从同一 block 确定性重建 GlideEnvelope、AerodynamicTable 和 ForceMomentClosure definition。

四条真实连接具有独立语义：aero asset 以 `AssetBinding` 在 prepare-time 进入 AerodynamicTable prepared model；CAVH GlideEnvelope 和 YYZ AerodynamicTable 以 `PureQuery` 进入正式 algorithm consumer；YYZ ForceMomentClosure 以 `ContinuousClosureLink + IntervalModel` 进入 RigidStep。每条 entry 保存 tagged provider/consumer endpoint、exact contract、两端 cardinality、有效 `SourceRef` 和适用的 scope/asset/temporal facts。CAVH query 不生成 scope proof，YYZ query/closure 只在两端显式声明同一 Vehicle scope 后生成 exact scope resolution。缺失、多重、contract/kind/scope/temporal 不兼容和缺失来源位置均返回直接诊断。

`hash_canonical_mission_ir` 的 `semantic-bytes@2` 对规范化 graph 生成 tagged、length-prefixed、big-endian semantic bytes 与 SHA-256；source location、声明顺序和 plan id 被排除。独立 Python reference 与 C++ vector 一致，并实际覆盖 scoped algorithm consumer、typed ports 和 binding intents；非 canonical order、非有限值和负零会在 hash 前失败。Compiler production header 不引用具体 package，CAVH formula 与 YYZ rigid-step 仍处于 algorithm consumer 分区。

当前没有 syntax-neutral `SourceTree`/`SourceMap`、完整 `PlanProofIndex`、link image、持久化 serialization、RuntimeComponent 或 Session。plan 仍不能重建完整 PreparedModel。StateOwner、DecisionAuthority、activation/topology、intervention/fault routing 与 RuntimeComponent sampled graph cycle analysis 等待首个对应 consumer。
