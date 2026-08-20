# compiler

职责：SourceTree/SourceMap、Catalog view、Canonical Model Graph、binding、proof、lowering、ExecutionPlanDescriptor、exact linker 和静态 Image conformance。

允许依赖：foundation、contracts、model_sdk descriptors。

禁止依赖：具体 package 实现、Session state、Artifact format 和前端对象。

当前保留两条静态编译 API。既有 programmatic `TypedStaticCompositionSource` 继续形成 revision 2 `CanonicalMissionIr`、typed `BindingPlan`/`TemporalBindingPlan`、portable query/closure execution inputs和窄 `ExecutionPlanDescriptor`，用于已接受的YYZ/CAVH qualification。新增 `CompleteStaticCompositionSource` revision 3为最小REF-YYZ图形成 `CompleteCanonicalMissionIr`、descriptor revision 5、派生 `PlanProofIndex`，再以exact package implementation table链接为Contracts拥有的process-local `ExecutionPlanImage` review artifact。

正式 `Catalog::build` 是两条路径唯一的descriptor validator。Catalog对PureQuery/Closure/RuntimeComponent payload实施封闭互斥，精确校验placement、ports、state owner/schema、initial/evolution、obligation entries、phase/schedule/temporal、lifecycle、request/result contracts与workspace。当前Closure只接受有产品证据的`FrozenInterval + IntervalModel`；CandidateState/AlgebraicSolve继续拒绝。YYZ Catalog已包含uniform environment、aero、closure、RigidBody、Mass、guidance、controller、actuator、config-driven propulsion和terminal evaluator的真实产品合同。

完整路径把source occurrence/config/assets、typed provider-consumer edges、两个唯一state owner/writer、initial/projection/evolution obligations、QueryPlan、ClosurePlan、invocation authorization、RuntimeComponent callsites、regions/Boundary DAG、IntegrationScope、Rigid/Mass Transaction和terminal committed-history plan全部冻结。consumer binding只描述result flow；调用权必须来自package obligation的invocation requirement与source binding。asset proof仍只证明source-selected非空identity preservation，不声明资产存在、可达、内容hash或payload已解析。

`hash_canonical_mission_ir`的`semantic-bytes@2`与独立Python reference、qualification vector保持原样，RuntimeComponent仍被该API拒绝。完整图使用additive `semantic-bytes@3`：model/version/config/assets/ports/state schema/obligation/phase/contracts/schedule/temporal/invocation composition进入source semantic hash；package entry identity/version、recipe、workspace、state layout、build fingerprint、typed callable/address和SourceRef location被排除。source relocation只改变provenance；declaration/registration reorder不改变canonical结果；implementation selection变化会改变descriptor/image而不污染source semantic hash。

`PlanProofIndex`由真实provider/cardinality/scope/temporal、owner/writer、authorization、region/DAG/topology、integration/transaction/evaluator和entry facts派生；linker重算required proof并对缺失、篡改或implementation/signature/layout/build lock不匹配fail closed。Image保存exact package/build lock、numeric handles、deterministic process-local slot layout、Definition builder、typed RuntimeCellFactory、state/slot codec和其他type-preserving entry references；Query invocation标为`CallerLocal`且没有result slot，FrozenInterval Closure只有一个held slot和writer token。link阶段不调用entry，也不创建PreparedModel/Bound handle/workspace/RuntimeCell/per-session state。Compiler production header不引用具体package；exact package set只由composition root/test注入。

当前仍没有syntax-neutral `SourceTree`/`SourceMap`、通用`PreparedModelKey`/共享cache、持久化serialization或Session。complete API形成planning/proof/link review artifact，并已精确冻结R3所需的package-owned factory/codec静态选择、result route、fixed RK4 scope、transaction branch与Session-owned initialize-time/no-shared-cache lifecycle，但Image本身不执行也不拥有Session对象。R3才由package/generated composition恢复typed entry、调用factory/codec/science entry、物化Prepared/Bound/runtime objects、调度、积分、暂存candidate并commit；本轮不引入统一callback mini-runtime，也不以model-id switch、signature解析或telemetry-as-authoritative-flow补执行层。`GNC-PLAN-RUNTIME-COMPONENT-UNAVAILABLE`保留为旧窄API和残缺图的防线；DecisionAuthority、activation/topology、intervention/fault routing、Observation/Encoding与更广泛sampled graph能力等待各自真实consumer，G3未通过。
