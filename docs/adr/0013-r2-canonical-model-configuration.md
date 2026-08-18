# ADR-0013: R2 canonical model configuration block v1

- Status: Accepted
- Date: 2026-08-18
- Owner: Repository owner
- Related task: R2-IR-001
- Architecture references: 05 §8～§10、12 §5.1、§5.2

## Context

R2 Compiler 已有真实 YYZ/CAVH package descriptor 与 typed composition source。ForceMomentClosure 的 body frame、clock domain、configuration revision 和 `NumericalPolicy` 需要成为 canonical graph 的稳定事实；CAVH GlideEnvelope 与新增 YYZ AerodynamicTable PureQuery 也需要通过同一最小值类型表达真实 definition 配置。Compiler 需要保持 package-neutral，具体 definition 的物理约束与构造仍由 package 负责。

## Decision

1. `CanonicalConfigBlock` v1 由 exact schema id、正整数 schema version 和按 field id 排序的完整字段集合组成。字段值只允许 UTF-8 string、signed 64-bit integer、enum token 和 IEC 60559 binary64。
2. canonical 字段集合严格匹配 `StaticModelDescriptor` 的 package-owned schema。缺项、多项、重复项、类型差异、空 string/enum、非有限 binary64 与负零均在 IR 构建阶段拒绝。
3. source 值、fixture 值和 package default rule 均可形成字段；每个字段保留一个直接 `SourceRef`。这些位置用于诊断与 provenance，不参与 canonical semantics。
4. package 提供确定性 builder 与反向 canonical projection。相同 block 必须重建相同 typed definition；字段值变化进入 canonical semantics。当前直接 consumer 为 YYZ ForceMomentClosure、YYZ AerodynamicTable 和 CAVH GlideEnvelope。
5. asset payload 与 asset identity 保持独立。配置 block 表达 definition 配置，`CanonicalAssetBinding` 表达 role、asset schema 和 asset id。
6. schema 中字段含义、单位、类型或必需性变化时升级 schema version。typed definition 的科学语义或执行契约变化时同时按 package policy 升级 model version。仅 provenance 位置变化不升级版本。
7. wire compatibility、migration、持久化 serialization、完整 runtime diagnostics、Session 与 RuntimeComponent 不进入 v1。

## Consequences

- Compiler 可以用通用规则冻结 configuration identity，并保持对具体 YYZ/CAVH 类型零依赖。
- package builder 继续承担 frame、clock、revision、数值策略与科学参数的领域校验。
- 当前 schema 是 in-process canonical IR contract；未来 frontend 可以投影到同一 block，仍需单独决定 wire 表示和迁移策略。

## Alternatives considered

- 在 Compiler 内按 package id 构造领域 definition：会引入 package-specific 分支并破坏依赖边界。
- 只保存自由文本 map：无法固定值类型、binary64 规范性和确定性重建。
- 把 asset payload 混入配置：会模糊 definition 配置与独立 asset identity 的边界。

## Executable evidence

- `framework/include/gnc/model_sdk/static_descriptor.hpp`
- `packages/yyz-rigid-step/include/yyz/rigid_step.hpp`
- `packages/cavh-formula/include/cavh/formula.hpp`
- `r1.yyz-rigid-step.probe`
- `r1.cavh-formula.probe`
- `r2.compiler-static-plan.probe`

## Supersession rule

首个需要跨进程编码、schema migration 或新增公共值类型的真实 consumer 可以重新开启本决定，修订范围只覆盖该 consumer 所需语义。
