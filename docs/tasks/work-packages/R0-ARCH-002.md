# R0-ARCH-002｜依赖、语义与演进守卫准备包

- 状态：Prepared — activation pending
- Backlog 状态：`planned`（本准备切片不激活任务）
- Assignee：未指派
- Owner role：Architecture Lead
- 准备人：Codex
- 准备日期：2026-08-10
- 依赖：`R0-ARCH-001` 与 `R0-SPEC-001` 均已为 `done`
- 关联 gate：G0

## 准备结论

已接受的 R0-ARCH-001 基线证明术语/authority 唯一性、ADR-0003/CMake module DAG、Legacy ownership 与派生 hash 一致性；15 项 baseline mutation、35 项 review-contract mutation 与 76 项 acceptance mutation 均进入真实失败路径。仓库总验证另有少量 Legacy include/token 和 `kernel -> compiler` 检查。现有证据尚未覆盖 source include 方向、完整 Legacy 删除词、延期能力禁入、精确 exception 规则和未来 descriptor/state/transaction 语义。

candidate 的 27 个名称 / 33 项职责已作为准备输入完成归类：22 项落在当前 primary owner 或 secondary consumer 内，3 项提出新的物理 owner 细分，2 项路由到 `packages_user` 逻辑贡献边界，另 6 项属于 5 个当前 glossary 未登记名称。Accepted `RECON-DEC-006` 固定 `packages_user` 与 `composition_root` 为 source/composition rule label；Accepted `RECON-DEC-007` 保留 22 条现有 owner/consumer mapping，并将其余分类留给带 glossary migration、superseding ADR 与 registry version 的后续切片。

空骨架缺少 RuntimeComponent、StateSchema、ExecutionPlanDescriptor、StepTransaction、Artifact 与 Workflow task 等目标 artifact。对这些对象做字符串搜索只能产生弱证据。本任务激活后应先交付可在 R0 证明的物理/治理守卫，并给其余 fitness function 绑定明确的 prerequisite artifact 与启用 gate。详细映射见 [架构 fitness 覆盖与故障设计](../../quality/architecture-fitness-coverage-plan.md)。

## 权威输入

- `docs/tasks/backlog.json` 中的 `R0-ARCH-002`；
- `AGENTS.md` 的 R0 边界、实现纪律与 repository guard 要求；
- `docs/adr/0003-initial-module-dependency-dag.md` 的 module DAG；
- `docs/adr/0005-derived-architecture-baseline.md` 的单一 authority 与派生证据规则；
- `design-notes/gnczmkn-architecture-roadmap/11-roadmap-overview.md` §13；
- `design-notes/gnczmkn-architecture-roadmap/roadmap/migration-governance-and-acceptance.md` §9、§13–§19；
- `design-notes/gnczmkn-architecture-roadmap/roadmap/r0-r2-foundations.md` §2.6–§2.10；
- `docs/architecture/authority-registry.json`、`CMakeLists.txt` 与当前 production roots；
- `tools/modules/ArchitectureBaseline.psm1`、`tools/validate-architecture-baseline.ps1` 和 `tools/verify-repository.ps1` 的实际覆盖。

## 依赖闭合后拟交付的首个纵向切片

1. 定义治理专用、无 runtime consumer 的 architecture-fitness policy 与派生报告；若 policy 成为公共 schema，先提交 ADR。
2. 从 authority registry 与 ADR-0003 派生 module/source-root 规则，统一检查 CMake edge 和 C/C++ `#include` 方向。
3. 对 `packages/`、`user/`、`apps/` 和 adapter composition roots 使用单独的边界规则，不把它们伪装成 framework module。
4. 对 Legacy include/path/runtime token、延期能力 token 和跨防火墙内部类型使用精确源文件扫描。
5. 将 exception 收窄到 rule id、精确 path、精确 token/edge、理由、owner role 与 expiry gate；禁止目录级和正则级宽泛放行。
6. 在内存或临时目录中构造负向 source inventory，证明每个当前启用的规则能拒绝代表性违规；负向测试不得修改 production tree。
7. 生成 coverage report，分开记录 policy 的 `enforced`、`not-applicable-awaiting-artifact`、`deferred-by-gate` 与 evaluator 的 `passed`、`failed`、`not-run`；禁止把零目标文件当作通过。
8. 接入 CTest、`verify-repository.ps1`、Debug/Release bootstrap 和固定双平台 CI。

## 首切片保持零修改

- `framework/`、`packages/`、`adapters/`、`apps/` 与 `user/` 的产品/项目源码；
- `reference/legacy/` archive、manifest、reproduction evidence 与 extracted source；
- ADR-0003 的 module 集合与依赖方向；
- R1–R8 descriptor、state、transaction、artifact 或 workflow 公共契约；
- backlog 中任何任务的状态与 assignee，直到依赖和 owner 审批闭合。

## 必测失败路径

| Case | 注入 | 预期规则 |
| --- | --- | --- |
| ARCH-MUT-001 | `model_sdk` source include `gnc/compiler/...` | package/model SDK 反向依赖失败 |
| ARCH-MUT-002 | `packages/` source include `gnc/kernel/...` | package 越过 Plan Firewall 失败 |
| ARCH-MUT-003 | `kernel` source include `gnc/compiler/...` | Kernel/Compiler 反向依赖失败 |
| ARCH-MUT-004 | `kernel` source include CSV/tool/frontend header | Kernel format/domain 泄漏失败 |
| ARCH-MUT-005 | `workflow` source include `CommittedStateStore`/`CycleFrame` internal header | Workflow/Commit Firewall 越界失败 |
| ARCH-MUT-006 | adapter 直接 include Kernel internal state header | Adapter 绕过 Application/Control 边界失败 |
| ARCH-MUT-007 | framework source include `user/...` | project code 反向进入 framework 失败 |
| ARCH-MUT-008 | production source include `reference/legacy/...` | Legacy build/runtime 依赖失败 |
| ARCH-MUT-009 | production identifier 使用 `SimulationNode` 或 `NodeRegistry` | Legacy deletion token 失败 |
| ARCH-MUT-010 | production 定义 `TopologyTransaction`、`SegmentTransaction` 或 dynamic package runtime | gate 前半实现失败 |
| ARCH-MUT-011 | CMake 增加未登记 module | ADR/physical graph 漂移失败 |
| ARCH-MUT-012 | CMake 增加 closure 外 edge | module dependency 失败 |
| ARCH-MUT-013 | exception 使用目录 glob、通配 token 或无 expiry | 宽泛 allowlist 失败 |
| ARCH-MUT-014 | 违规 include 使用相对路径、路径归一化或大小写变体 | 旁路规范化失败 |
| ARCH-MUT-015 | 把 `packages_user` 或 `composition_root` 加入物理 module graph | 逻辑边界未经 ADR 提升失败 |
| ARCH-MUT-016 | 向 authority registry 注入未评审 responsibility overlay 或 candidate-only Legacy owner | registry shape / glossary closure 失败 |

## 误报与漏报失败路径

- 扫描 Markdown、ADR、fixture expected text 或冻结 Legacy 内容并把历史术语当成生产依赖会失败；
- 只匹配 `target_link_libraries(kernel ... compiler)`，遗漏 source include、transitive edge 或其他 module pair 会失败；
- 只搜索名字而不解析 include/module owner，无法区分注释、字符串、声明与依赖时不能宣称强保证；
- production root 没有目标文件时返回 `passed` 会失败，必须报告零目标与对应 maturity；
- 新增真实 artifact 后未把相关规则从 awaiting-artifact 升级为 executable 会失败；
- guard 自身引用 Legacy/延期 token 时依赖全局排除会失败，应使用精确文件/规则边界；
- mutation 只改变预期报告文本，没有经过与正向检查相同的 evaluator 会失败；
- 自动修复、删除或移动用户文件会失败，validator 只允许读取与生成受控报告。

## 激活前置条件

1. `R0-ARCH-001` 与 `R0-SPEC-001` 由有权 reviewer 关闭为 `done`；
2. Architecture Lead 指派本任务 assignee 与 reviewer；
3. ADR-0005 的 authority/baseline 方案被接受，或 replacement ADR 已落档；
4. Product Owner/Architecture Lead 明确新的 governance-only policy 是否需要 ADR；
5. 固定双平台 CI 已可产生并保存 hosted evidence。

满足前置条件后，先把 backlog 状态改为 `in_progress` 并填写 assignee，再实现首切片。准备文档本身不能作为任务 deliverable 完成证据。

## 首切片验收证据

- 当前 repository positive scan 通过，并报告每个 root 的实际 source/file count；
- ADR/CMake DAG、source include DAG、Legacy/deferred token 与 exception policy 使用同一确定性 report；
- 上表至少 16 个 mutation 全部由生产 evaluator 拒绝；其中 015/016 复用已经由 ARCH-001 证明的 authority evaluator；
- 每个启用规则至少有一个实际目标或一个通过同一 evaluator 的负向 mutation；
- future semantic rule 显式记录 prerequisite artifact、owner task 与 activation gate；
- Debug/Release CTest、repository verification、双平台 hosted CI 与 `git diff --check` 通过；
- 产品源码和冻结 Legacy 保持零修改；
- Architecture Lead reviewer 对 rule coverage、exception 与 deferred activation 做具名审查。

## 准备切片评审记录

- 实现自审：Codex，2026-08-10；结论为“准备范围闭合，可在依赖关闭后直接领取；未产生实现完成声明”。本自审不替代 Architecture Lead reviewer。
- 权威覆盖：逐项核对治理分册 §9，覆盖 FF-ARCH-01～16、FF-DEP-01～09、FF-OBJ-01～15、FF-BEH-01～10、FF-PLAN-01～13、FF-RUN-01～10、FF-DIA-01～04、FF-ART-01～03、FF-CFG-01～03 与 §9.8 deletion guard；显式 inventory 核对为 84/84。
- 当前证据审计：记录 architecture baseline 的 15 项 mutation、review contract 的 35 项 mutation、task acceptance 的 76 项 mutation、CMake DAG/authority 强覆盖和 repository regex 的 partial 覆盖；所有 product-artifact 语义保持 awaiting/deferred，没有借零文件数量声明通过。
- 故障设计：ARCH-MUT-001～016 覆盖 module/include、Plan/Commit/Application 防火墙、Legacy、延期能力、ADR graph、逻辑/物理边界、registry overlay、exception 和路径规范化；要求 mutation 与 positive scan 共用 evaluator。
- 自审修正：Adapter include 上限从 ADR transitive closure 收窄为 Application/稳定 contract/Artifact DTO；FF-ARCH-02 完整规则改为等待真实 Kernel dispatch，只把已知 token 子守卫放入 R0 首切片；policy state 与 evaluation result 改为两个正交字段。
- 状态边界：`R0-ARCH-002` 仍为 `planned`、assignee 为空；`R0-ARCH-001` 与 `R0-SPEC-001` 均已完成 commit-bound acceptance；没有修改产品源码或冻结 Legacy。
- 验证：Debug bootstrap 通过，9/9 CTest 与 repository verification 通过；45 个 JSON、65 个 task entries、85 个 Markdown 在准备文件加入后通过检查。
- 开放审查：Architecture Lead 仍需确认 public/private include seam、exception policy、governance-only policy 是否需要 ADR，以及依赖关闭后的首切片 assignee/reviewer。
- reconciliation amendment：已接受的 `R0-ARCH-001` 确定性 authority validator、review contract 与 acceptance guard 分别拒绝 15/15、35/35 与 76/76 mutation；Debug/Release CTest 与 repository verification 通过。`R0-ARCH-002` 仍为 `planned`、assignee 仍为空，未生成 architecture-fitness pass 声明。
