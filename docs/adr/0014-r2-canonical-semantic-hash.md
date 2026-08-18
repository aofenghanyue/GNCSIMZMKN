# ADR-0014: R2 canonical graph semantic bytes and SHA-256

- Status: Accepted
- Date: 2026-08-18
- Owner: Repository owner
- Related tasks: R2-IR-001、R2-BIND-001
- Architecture references: 05 §8～§10、12 §5.1

## Context

CanonicalMissionIr 已覆盖真实 entity、Vehicle scope、model occurrence、package/model/preparation identity、placement、typed configuration、asset binding、ports、algorithm consumer 与 binding intent。R2 需要一个与 source 表示无关的 graph semantic hash，供后续静态 Compiler 切片识别等价语义。human-readable explain 保留 dry-run 用途，其格式不承担 canonical codec 职责。

## Decision

1. semantic byte encoding identity 固定为 `gnc.canonical-mission-ir.semantic-bytes@2`，hash algorithm 固定为 SHA-256。`@2` 增加 algorithm consumer scope，以及 model/algorithm port 的 binding kind、cardinality 与 temporal relation；`@1` 没有持久化 consumer，不提供隐式迁移。
2. record、string、uint32、signed int64、enum、binary64、collection 和 optional 分别带显式 type tag；固定 enum domain/value 与 config enum token 再使用不同 subtype tag。string 与 collection 使用 big-endian uint32 长度；整数、enum domain/value 与 binary64 bit pattern 使用 big-endian 固定宽度。
3. collection 按 canonical key 唯一排序。hash 前重新验证 IR 的 canonical order、identity、model/algorithm scope 关系、typed port、配置值、asset 与 binding endpoint compatibility。非 canonical graph 在进入 SHA-256 前失败。
4. hash 覆盖 mission revision/id、entity、scope、model/package/preparation identity、execution form、placement、model/algorithm scope relation、port binding kind/cardinality/temporal relation、canonical config、asset binding、algorithm consumer 与 binding intent。
5. `SourceRef` URI/path、输入声明顺序、plan id、指针、runtime instance、cache、Session 和 provenance 容器不进入 semantic bytes。
6. 当前 encoding 只服务 canonical graph identity。持久化 serializer、wire protocol、migration、plan linker 和 Artifact Store 保持未实现。

## Consequences

- 等价 source 位置、声明顺序和 plan id 产生相同 digest。
- entity、scope relation、placement、model、typed port、config、asset、algorithm consumer 或 binding intent 的真实语义变化会改变 digest。
- binary64 负零被视为非 canonical，避免同一数值语义出现两个 bit encoding。
- encoding 版本变化会形成新的 identity；旧 digest 不做隐式迁移。

## Alternatives considered

- 对 explain 文本直接 hash：会把展示格式与 canonical codec 绑定。
- 对 source JSON 字节 hash：member order、空白和路径会污染语义 identity。
- 使用平台原生整数与浮点内存布局：无法形成跨实现稳定字节序。

## Executable evidence

- `framework/include/gnc/compiler/canonical_semantic_hash.hpp`
- `fixtures/ref-r2-canonical-ir/cases.json`
- `tools/r2_canonical_ir_hash_reference.py`
- `r2.compiler-static-plan.probe`
- `r2.compiler-canonical-semantic-hash.reference`

当前独立 YYZ qualification vector 为 `b29dc67f2a9e0bb36cb18a5e54a8c4830bdb0cae718fbf856646ba903892511b`，其中包含真实 RigidStep algorithm consumer、Vehicle scope 和 aero/closure binding intents。

## Supersession rule

新增 canonical graph 节点类型或发现编码碰撞、歧义、不可移植 bit 表示时升级 encoding identity，并提供新的独立 reference vector。
