# ADR-0005: Derived architecture baseline and authority index

- Status: Proposed
- Date: 2026-08-10
- Owners: Architecture Lead
- Related tasks: R0-ARCH-001, R0-ARCH-002
- Architecture references: reference-glossary、02 §13、01 §16

## Context

R0 需要让术语、模块依赖和 Legacy 迁移归属可被自动检查。直接手写一份完整 JSON 术语表会与现有 Markdown 注册表形成第二套语义定义；只检查文档文本又无法给后续 architecture guard 提供稳定输入。

共享 enum、key、identity 和 owner 的定义已经分散到职责最接近的分册，物理模块方向由 ADR-0003 冻结。当前缺口是机器可读的唯一归属和可检测的派生投影。

## Decision

`reference-glossary.md` 继续作为唯一名称、状态、定义、退出关系和 Legacy 目标语义的登记权威。ADR-0003 继续作为模块依赖方向的权威，根 CMake 文件是当前物理 target graph 的事实。

仓库增加小型 `authority-registry.json`，每个共享 symbol 只登记一次语义权威文档和物理 owner；每个 Legacy 名称只登记一个 primary owner，可附带只读 consumer。该索引不复制 enum 值、key 字段或 Legacy 目标语义。

完整 `architecture-baseline.json` 与 conformance report 由上述输入确定性生成。派生物包含源路径和按 UTF-8、LF、无 BOM 归一化计算的 SHA-256，并记录生成器与入口脚本的同类 hash；检查模式会重新生成后比较。它们只用于治理、审查和测试，不被产品 runtime 读取。

## Consequences

- Positive: 人类可读定义保持单一，自动检查获得稳定、可追踪输入。
- Positive: CMake 实际边可以与 ADR 允许依赖的传递闭包逐边比较。
- Positive: Legacy 拆分仍有唯一 primary owner，跨模块消费者不会变成共同写入者。
- Costs: 权威 Markdown 表结构和 ADR dependency block 成为受校验的 authoring contract。
- Risks: PowerShell parser 只支持当前明确表格与 dependency notation；格式升级必须带反例和版本调整。
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

## Supersession rule

修改模块集合或依赖方向时以 superseding ADR 更新 graph version；共享 symbol 的语义 owner 改变时同时修改权威文档、归属索引和迁移说明。若派生 JSON 进入 runtime，则必须另行定义公共 schema、兼容策略和 production parser。
