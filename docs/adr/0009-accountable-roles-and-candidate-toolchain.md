# ADR-0009: Accountable roles and candidate toolchain

- Status: Accepted
- Date: 2026-08-10
- Amended: 2026-08-12
- Owners: Product Owner, Architecture Lead
- Related tasks: R0-GOV-001
- Architecture references: 00 §2、team operating model、D-003

## Context

R0-GOV-001 要求每个 required role 都有 assignee 与 reviewer，并要求 Product Owner、Architecture Lead 接受平台与编译器选择。原记录把有效责任主体限定为可解析到真人的身份，并将所有 Codex 或机器人席位视为占位符。当时没有仓库所有者对机器 actor 的授权证据，这项 fail-closed 判断与当时证据一致。

2026-08-12，仓库所有者在共同 Codex thread 中明确授权四个独立机器 actor 在 R0 范围内承担项目角色、作出决定、实现产物、开展独立复核并执行经授权的 Git/PR 流程。授权原文的 SHA-256、仓库与可信基线、共同 thread id、actor id、实际 task path、角色范围及持续约束记录在 [`r0-owner-authorization.json`](../governance/r0-owner-authorization.json)。该授权已经消除机器身份本身的资格阻塞。actor 仍须公开标记为 `machine_agent`，且不得伪装成人类。

本修订取代本 ADR 早先的 human-only 资格条款，也取代 R0 治理材料中与本次明确授权冲突的同类条款。未登记的 `Codex` 泛称、无法解析到授权记录的 `codex-*` 字符串、TBD、unknown 与其他占位值继续无效。授权记录只改变责任主体资格；Hosted CI、rights/provenance、外部分发、任务验收和阶段门仍由各自证据与规则控制。

D-003 的默认约束是 C++17、CMake 3.20 和 Windows/Linux smoke CI。项目声明 CMake 3.20+，原有 `CMakePresets.json` 却使用 schema version 6；CMake 3.20 文档支持 schema version 2 以及当前使用的 configure、build、test preset 字段，而 schema version 6 在 CMake 3.25 才出现。[CMake 3.20 presets](https://cmake.org/cmake/help/v3.20/manual/cmake-presets.7.html) [CMake 3.25 release notes](https://cmake.org/cmake/help/v3.25/release/3.25.html)

原有 CI 使用 `ubuntu-latest`、`windows-latest` 和浮动 checkout tag。GitHub 说明 `-latest` 是 GitHub 提供的最新稳定镜像，并可能随迁移变化；runner-images 项目建议需要避免迁移时使用明确 OS 标签。镜像内容仍会滚动，因此每次运行还需保存 image、compiler 和工具的精确身份。[GitHub-hosted runners](https://docs.github.com/en/actions/reference/runners/github-hosted-runners) [runner-images](https://github.com/actions/runner-images)

Windows 本机已观察到 MSVC 19.50.35725、CMake 4.1.2-msvc8 与 Ninja 1.12.1；WSL Ubuntu 24.04 已观察到 GCC 13.3.0、CMake 3.28.3 与 Ninja 1.11.1。这些探测只构成候选验证证据。Visual Studio 18 2026 generator 从 CMake 4.2 提供，因此固定 Windows 托管 CI profile 需要 CMake 4.2+，同时不提高项目使用 Ninja 或其他可用 generator 时的 3.20 声明下限。[Visual Studio 18 2026 generator](https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2018%202026.html) [MSVC compiler versions](https://learn.microsoft.com/en-us/cpp/overview/compiler-versions?view=msvc-180)

## Decision

[`r0-owner-authorization.json`](../governance/r0-owner-authorization.json) 是本次 R0 机器 actor 资格和范围的授权记录。有效机器 actor 必须同时满足以下条件：

- actor id 能唯一解析到该记录；
- `kind` 与身份披露明确标记为机器智能体；
- actor 绑定共同 thread 中唯一且实际存在的 task path；
- 所承担角色落在该 actor 的 `authorized_roles` 内；
- 授权日期、来源 instruction SHA-256、仓库与 R0 scope 完整；
- 未触发授权记录中的 fail-closed boundary。

`docs/team/role-assignments.json` 需要通过有版本的后续修改引用授权 actor。每个角色继续显式包含 assignee、reviewer 与 required。assignee/reviewer 必须解析为不同 actor，且实际 task path 也必须不同。Scientific Authority 与 Architecture Lead 的 actor 和 task path 必须各自独立。同一事项的实现者与最终复核者必须使用不同 actor。字符串差异只能作为输入，validator 还要核验授权链、角色范围和 task binding。

本次授权登记四个 actor：`r0-po-agent`（`/root`）、`r0-architecture-agent`（`/root/r0_architecture_agent`）、`r0-science-agent`（`/root/r0_science_agent`）与 `r0-validation-agent`（`/root/r0_validation_agent`）。角色分配遵循授权记录中的 `authorized_roles`，主要复核关系依次由 Validation、Validation、Architecture 与 Product actor 承担，形成实现/最终复核分离。

`docs/governance/toolchain-support-matrix.json` 是 R0 治理证据，不是运行时或发布兼容性 schema。矩阵只设两个 `candidate-primary` profile：

- Windows x64、MSVC `>=19.50.0 <19.60.0`；本机以 Ninja 验证，托管 CI 使用 `windows-2025-vs2026` 与 Visual Studio 18 2026 generator；
- Ubuntu 24.04 x64、GCC `>=13.0.0 <14.0.0`；本机/托管 CI 均使用 Ninja，托管 runner 为 `ubuntu-24.04`。

两条 profile 都要求 C++17、Release、warnings-as-errors、CMake 3.20+、Python 3.8+ 和可运行治理检查的 PowerShell。Windows 托管 profile 因 generator 额外要求 CMake 4.2+。每个 CI run 必须输出 runner OS/architecture/name、image identity、compiler、CMake、generator、PowerShell、Python 与 Git 版本。镜像 family 固定不代表其每周 patch 固定。

CI checkout 固定到 `actions/checkout` v6.0.2 的 commit `de0fac2e4500dabe0009e67214ff5f5447ce83dd`，并登记进 provenance inventory。[checkout v6.0.2](https://github.com/actions/checkout/releases/tag/v6.0.2) [pinned commit](https://github.com/actions/checkout/commit/de0fac2e4500dabe0009e67214ff5f5447ce83dd)

w64devkit/GCC 16.2 只服务冻结 Legacy 复现，保持 `evidence-only` 且不具备产品资格。Clang、macOS、ARM、MinGW 产品构建、sanitizer、动态 package ABI 和 Python wheels 尚未 qualified。

候选 profile 只有在以下三项全部成立后才能改为 supported：本 ADR 由已授权的 Product Owner 与 Architecture Lead actor 接受；八个 required role 都有独立且可解析的 assignee/reviewer；固定 runner 的新 workflow 成功运行并保存精确环境证据。前两项已针对冻结提交 `d4f1a6b105b680a7e7f32925d90a44c0f85f57e0` 通过并记录在 [ADR disposition](../governance/adr-dispositions/ADR-0009-2026-08-12.json)。第三项由 push run `31559701566` 与 PR run `31559704268` 闭合，commit、checkout context、runner/image/tool/compiler identity、步骤结论、日志归档哈希和 90 天上游保留期均记录在 [Hosted CI evidence receipt](../quality/hosted-ci-evidence-R0-GOV-001.json)。两个 profile 因此进入 `supported`；未列入矩阵的工具链继续保持未资格化。

## Consequences

- Positive: 声明的 CMake 下限与 preset schema 重新一致。
- Positive: 本机探测、托管 runner family、compiler range 和每次 run 的精确身份分别表达，避免把滚动镜像误作 patch 承诺。
- Positive: CI 在 Release 与 warnings-as-errors 下覆盖全量 CTest 和 repository verification。
- Positive: 机器 actor 资格通过仓库所有者授权、显式身份类型、实际 task path 和角色 scope 形成可审计链路。
- Cost: Windows 本机 Ninja 验证与托管 Visual Studio generator 是两条证据路径，出现差异时需要单独归因。
- Cost: Ubuntu 本机 WSL 没有 PowerShell，只能提供 C++/CMake smoke；完整治理 CTest 需由托管 runner 补齐。
- Cost: 角色登记、validator、readiness report 与当前 handoff 材料必须在同一治理切片中迁移到授权 actor 语义。
- Risk: 固定 runner label 仍会更新预装工具，workflow 必须保留每次运行的 identity 输出。
- Governance boundary: 新 CI run 及其 retained evidence 已闭合工具链资格；任务仍须在本证据物化提交通过独立验收后单独标为 `done`。

## Alternatives considered

- 提高项目最低 CMake 到 3.25：可以保留 preset schema 6，但 D-003 尚未被有权角色接受，现有字段也不需要新版 schema。
- 继续使用 `*-latest`：配置较短，但 OS 迁移会改变候选平台边界。
- 固定 runner 内每个预装工具 patch：hosted image 持续滚动，此承诺无法由 runner label 保证。
- 把本机验证直接定义为正式支持：缺少角色接受与托管双平台运行证据。
- 把 Legacy w64devkit 用作产品 Windows profile：会跨越 Legacy evidence firewall，且 GCC 16.2 与目标 MSVC profile 不同。
- 延续 human-only 资格限制：旧限制在缺少授权时提供了安全默认值；2026-08-12 的仓库所有者授权已经提供可审计的机器 actor 身份、范围与独立性依据，继续沿用会拒绝有效责任主体。

## Verification

- 静态 validator 检查 owner authorization schema、来源 instruction SHA-256、四个 actor 的机器身份披露、共同 thread、唯一 task binding、角色 scope、双 actor 评审和科学/架构独立性；
- validator 对齐 C++17、CMake 3.20、preset schema 2、两个 compiler range 与所有排除项；
- workflow 检查固定 runner label、checkout commit、Release、warnings-as-errors、CTest、repository verification 和 identity 输出；
- mutation suite 拒绝缺 reviewer、同 actor 或同 task path 自审、Science/Architecture 共用 actor 或 task、未登记机器 alias、缺授权来源/scope/身份披露、机器 actor 伪装人类、无有效 disposition 的 Accepted、preset 漂移、latest runner、浮动 action、无上界 compiler、Legacy 晋升和空角色虚假 readiness；
- 本地 Windows Debug/Release 与 Linux GCC smoke 的命令和结果记录在工作包；固定 Hosted CI 的 push 与 PR merge-context 运行均已通过。离线 guard 会复算资格提交、工作流 Git blob、PR 临时合并 commit object、必需步骤和精确环境身份。

## Acceptance and supersession

`r0-validation-agent` 已对冻结提交的精确 diff、授权哈希、actor/task binding、独立性和 fail-closed boundary 完成复核；`r0-po-agent` 与 `r0-architecture-agent` 均给出 `accept-as-written`。正式 disposition 记录 reviewed commit、文件集哈希、decision actors、独立 reviewer、理由和验证结果。本 ADR 因此进入 `Accepted`。

新 workflow 在两个固定 runner 上成功并保留 commit-bound evidence 后，另行更新矩阵与报告，将经过证据覆盖的 profile 改为 supported。rights/provenance、外部分发与 gate decision 继续按各自 ADR、evidence 和 review 闭合。任何 OS、architecture、compiler family/range、CMake minimum、generator、warning policy、sanitizer、ABI 或发布目标变化都需要重新评审。
