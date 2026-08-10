# R0-ARCH-001｜术语与架构依赖基线

- 状态：Review
- Assignee：Codex
- Owner role：Architecture Lead
- 目标评审日期：2026-08-10
- 关联 gate：G0 / G1

## 权威输入

- `docs/tasks/backlog.json` 中的 `R0-ARCH-001`；
- `reference-glossary.md` 的唯一名称、状态、定义、权威、退出名和 Legacy 迁移表；
- `ADR-0003` 的初始模块集合、允许依赖边和禁止反向依赖；
- `02 §13` 的模块依赖规则与 `01 §16` 的 Legacy 迁移方向；
- 根 `CMakeLists.txt` 中当前可执行的模块 target graph。

## 权威输出

1. 从 Markdown 术语表确定性生成、带源文件哈希的机器可读术语与 Legacy 迁移基线；
2. 从 ADR-0003 确定性生成、带源文件哈希的模块依赖基线；
3. 共享术语、枚举和值域、稳定 key 与 owner authority 的唯一归属清单；
4. 校验派生物未漂移、identity 未重复、引用可解析、依赖图无环且 CMake 实际边未越界的 conformance 命令；
5. CTest 与 repository verification 中可追踪的 terminology conformance 证据。

## 失败路径

- 权威 Markdown 表头、列数或状态值改变而解析器无法识别时失败；
- 术语、退出名、Legacy 名称、共享 symbol 或模块 identity 重复时失败；
- 退出名或 Legacy 目标引用了未注册的目标术语且未声明为关系/组合时失败；
- 共享枚举或稳定 key 没有唯一 authority、owner 或对应注册术语时失败；
- 派生 JSON 的源文件路径、SHA-256、内容或排序与权威输入不一致时失败；
- authority registry 顶层、module、shared-symbol 或 Legacy ownership 行出现未注册字段时失败，防止责任 overlay 被解析器静默忽略；
- 模块依赖包含未知模块、自依赖、环或 ADR-0003 未允许的边时失败；
- CMake module target 或 `target_link_libraries` 实际依赖偏离 ADR-0003 时失败；
- Kernel 依赖 Compiler，或具体 package 越过 composition boundary 时失败；
- `packages_user` / `composition_root` 未经 ADR-0003 变更被提升为物理 module，或未在 glossary 注册的 Legacy 名称被加入 ownership registry 时失败。

## 验收与证据

- `tools/validate-architecture-baseline.ps1` 输出术语、alias、Legacy 映射、共享 symbol、模块和依赖边计数；
- 默认检查模式验证仓库中派生 JSON 与重新生成结果字节一致；
- 自动反例覆盖重复 identity、悬空引用、循环依赖、禁止边、源哈希漂移、注册表形状漂移、逻辑边界物理化和无 glossary 行的 Legacy ownership；
- CTest 包含独立 architecture baseline conformance test；
- `tools/bootstrap.ps1` 全量通过；
- `git diff --check`、变更审查清单和最终 commit hash 作为代码评审证据。

## 保持零修改

- `framework/`、`packages/`、`adapters/`、`user/` 与 `reference/legacy/`；
- R1–R8 产品契约和运行能力；
- Compiler、Kernel、Session、Artifact Store 与前端路径；
- `reference-glossary.md` 和 ADR-0003 的语义内容，除非校验发现权威输入本身冲突。

## 升级触发器

- 修改 ADR-0003 的允许依赖边或引入新模块；
- 修改共享 enum/key 的公共兼容规则；
- 引入第三方 Markdown parser、图算法库或新的运行时依赖；
- 把派生 JSON 直接作为 runtime API 或运行时配置读取。

## 评审记录

日期：2026-08-10。

- terminology review：276 个 canonical terms、20 个退出/关系 alias 和 10 个 capability rows 全部可解析，9 个共享 enum 的成员集合与唯一语义权威逐项一致；
- ownership review：27 个共享 enum/key/owner symbols 各有一个语义权威和一个物理 owner，22 个 Legacy 名称各有一个 primary owner；
- dependency review：ADR-0003 的 9 模块 DAG 无环，CMake 的 22 条直接依赖均落在允许闭包，Kernel 对 Compiler 依赖保持为零；
- failure review：原有 9 个反例，加上未知顶层字段、module 行字段、shared-symbol 行字段、Legacy responsibility overlay、`packages_user` / `composition_root` 物理模块提升，以及 5 个 candidate-only Legacy 名称共 6 组反例，合计 15 个 mutation 均被拒绝；
- reconciliation review：candidate 的 27 个名称 / 33 项职责被完整分成 22 项现有 owner/consumer 对齐、3 项物理 owner 细分提案、2 项逻辑 contribution-boundary 路由和 6 项属于 5 个当前未注册名称的职责；33 项引用的目标术语均已存在，但 owner 粒度与名称注册仍需 Architecture Lead / Validation Lead 决策；
- review findings：修复 5 项职责分册规范名称缺失或权威错指；统一 glossary 与 03 的 17 个 `NumericalStatus` 成员；清除 14 中 `CurrentCycleSample`、`PreviousCommittedSample`、`HeldLatestSample` 三个退出写法；修复 Markdown 分隔行列数未校验的问题；
- portability review：生成器模块保持 ASCII，派生 JSON 使用紧凑稳定序列化，文本源 hash 统一按 UTF-8、LF、无 BOM 归一化；
- verification：Windows PowerShell 5.1 targeted architecture validation 拒绝 15/15 个自动反例；MSVC Debug 与 Release 各 9/9 CTest 通过；repository verification 验证 56 个 JSON、65 个 task entries 与 98 个 Markdown；`git diff --check` 通过；
- residual risk：全库新增 CamelCase/代码词扫描与 include evolution guard 属于 `R0-ARCH-002`；ADR-0005 在 Architecture Lead 指派前保持 `Proposed`，任务因此停留在 `review`。
