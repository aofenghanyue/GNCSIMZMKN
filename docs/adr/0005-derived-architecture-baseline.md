# ADR-0005: Derived architecture baseline and authority index

- Status: Accepted
- Date: 2026-08-10
- Owners: Architecture Lead
- Related tasks: R0-ARCH-001, R0-ARCH-002
- Architecture references: reference-glossary、02 §13、01 §16
- Accepted disposition: [ADR-0005-2026-08-12](../governance/adr-dispositions/ADR-0005-2026-08-12.json)
- Reconciliation dispositions: [RECON-DEC-006](../governance/reconciliation-dispositions/RECON-DEC-006-2026-08-12.json)、[RECON-DEC-007](../governance/reconciliation-dispositions/RECON-DEC-007-2026-08-12.json)

## Context

R0 需要让术语、模块依赖和 Legacy 迁移归属可被自动检查。直接手写一份完整 JSON 术语表会与现有 Markdown 注册表形成第二套语义定义；只检查文档文本又无法给后续 architecture guard 提供稳定输入。

共享 enum、key、identity 和 owner 的定义已经分散到职责最接近的分册，物理模块方向由 ADR-0003 冻结。当前缺口是机器可读的唯一归属和可检测的派生投影。

## Decision

`reference-glossary.md` 继续作为唯一名称、状态、定义、退出关系和 Legacy 目标语义的登记权威。ADR-0003 继续作为模块依赖方向的权威，根 CMake 文件是当前物理 target graph 的事实。

仓库增加小型 `authority-registry.json`，每个共享 symbol 只登记一次语义权威文档和物理 owner；每个 Legacy 名称只登记一个 primary owner，可附带只读 consumer。该索引不复制 enum 值、key 字段或 Legacy 目标语义。

完整 `architecture-baseline.json` 与 conformance report 由上述输入确定性生成。派生物包含源路径和按 UTF-8、LF、无 BOM 归一化计算的 SHA-256，并记录生成器与入口脚本的同类 hash；检查模式会重新生成后比较。它们只用于治理、审查和测试，不被产品 runtime 读取。

### 2026-08-12 R0 accepted amendment

`RECON-DEC-006` 的接受结果为 `logical-only-keep-current`。`packages_user` 与 `composition_root` 仅作为 02 §13 的 source/composition rule label：前者描述 `packages/`、`user/` 经 Model SDK descriptor 接缝贡献能力，后者描述同时看到 packages、adapters 与 Application host 的组合位置。两者不进入 ADR-0003/1 的九模块集合、`authority-registry/1` 的 module/owner 字段或 CMake interface-module identity。改变该边界需要 superseding ADR 与 registry schema version 变更。

`RECON-DEC-007` 的接受结果为 `keep-current-22-owner-consumer-map`。审计中的 33 项 candidate responsibility 已按 `22 + 3 + 2 + 6` 完整分类：22 项与当前 owner/consumer 对齐；3 项 owner split 暂不导入；2 项进入上述逻辑路由并留给 `R0-ARCH-002` 的 source-boundary guard；6 项属于 5 个尚无 glossary §9 migration row 的名称。v1 registry 保留 22 条现有映射，不增加 responsibility overlay。后续新增名称或细粒度 owner 需要先登记 glossary migration、提交迁移证据并修订 ADR/schema。

`docs/architecture/r0-architecture-review-contract.json` 为本轮机器审查锁。它精确记录九模块顺序、两个逻辑标签、33 项分类、11 个权威/生成输入的规范化 bytes 与 SHA-256、零 runtime consumer 范围及 35 个必需 mutation。该锁只覆盖列出的术语、ownership、ADR-0003、物理分区、CMake 与派生物 source set；全库 include 方向、未知 CamelCase、未来 descriptor/state/transaction fitness 继续归 `R0-ARCH-002`。

Architecture Lead 与独立 Validation Lead 已对技术提交 `29f455efebd72113c1d311bc674a78c638265f34` 及文件集 SHA-256 `16d566512cd3d0bdb8e4f9fc84f3c8709328708aba7b4f95b4570b8a3f6a9561` 分别给出 `accept-as-written` 与 `approved`。上述 disposition、两项 reconciliation record 和任务接受记录共同固定本 amendment；`R0-ARCH-002`、rights/provenance、G0/G1 与 R1 解锁边界继续独立闭合。

## Consequences

- Positive: 人类可读定义保持单一，自动检查获得稳定、可追踪输入。
- Positive: CMake 实际边可以与 ADR 允许依赖的传递闭包逐边比较。
- Positive: Legacy 拆分仍有唯一 primary owner，跨模块消费者不会变成共同写入者。
- Costs: 权威 Markdown 表结构和 ADR dependency block 成为受校验的 authoring contract。
- Risks: PowerShell parser 只支持当前明确表格与 dependency notation；格式升级必须带反例和版本调整。
- Scope: `conformant` 只表示审查锁列出的 source set 与派生投影一致，不表示全库源码依赖、命名或未来 R1 contract 已完成。
- Modules kept unchanged: `framework/`、`packages/`、`adapters/`、`user/`、`reference/legacy/`。

## Alternatives considered

- 手工维护完整 JSON：更新成本低，但会产生可漂移的第二份术语定义。
- 运行时动态解析 Markdown：无需派生文件，却把治理格式耦合进产品能力。
- 引入 Markdown/graph 第三方库：当前许可证与依赖策略未冻结，R0 的表格与 DAG 规模也不需要该复杂度。

## Verification

- 术语、alias、Legacy、shared symbol 和 module identity 均做 ordinal 唯一性检查；
- shared symbol 的权威文件必须存在并包含该 symbol；
- Legacy 表和 ownership registry 必须一一对应；
- ADR 图无未知节点、自环或环，CMake target/edge 必须落在 ADR 允许闭包；
- 派生基线和报告与重新生成内容一致；
- 重复 identity、悬空 authority、遗漏 Legacy owner、依赖环、禁止边和 hash 漂移均有自动反例。
- capability section/header/identity/commitment/gate 与 authority reference 受结构校验；合法值之间的 ownership、status 与 source-root 漂移由权威快照拒绝；
- CMake module dependency 的未知 target、变量/generator 表达、非 `INTERFACE` visibility 与 ADR dependency block 漂移均 fail closed；
- 派生 JSON 必须逐字节 UTF-8、无 BOM、LF，BOM/CRLF mutation 会被拒绝；
- 产品路径对 baseline、registry、review contract 或 report 的 runtime 消费计数保持为零。

## Supersession rule

修改模块集合或依赖方向时以 superseding ADR 更新 graph version；共享 symbol 的语义 owner 改变时同时修改权威文档、归属索引和迁移说明。若派生 JSON 进入 runtime，则必须另行定义公共 schema、兼容策略和 production parser。
