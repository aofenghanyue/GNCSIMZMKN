# ADR-0009: Accountable roles and candidate toolchain

- Status: Proposed
- Date: 2026-08-10
- Owners: Product Owner, Architecture Lead
- Related tasks: R0-GOV-001
- Architecture references: 00 §2、team operating model、D-003

## Context

R0-GOV-001 要求每个 required role 都有 assignee 与 reviewer，并要求 Product Owner、Architecture Lead 接受平台与编译器选择。当前九个角色均未指派，其中八个为 required。仓库不能代替团队所有者推测姓名、邮箱或账号，也不能让 Codex 充当有权人类签字。因此本记录可以冻结约束和候选矩阵，但目前没有接受该决策的授权条件。

D-003 的默认约束是 C++17、CMake 3.20 和 Windows/Linux smoke CI。项目声明 CMake 3.20+，原有 `CMakePresets.json` 却使用 schema version 6；CMake 3.20 文档支持 schema version 2 以及当前使用的 configure、build、test preset 字段，而 schema version 6 在 CMake 3.25 才出现。[CMake 3.20 presets](https://cmake.org/cmake/help/v3.20/manual/cmake-presets.7.html) [CMake 3.25 release notes](https://cmake.org/cmake/help/v3.25/release/3.25.html)

原有 CI 使用 `ubuntu-latest`、`windows-latest` 和浮动 checkout tag。GitHub 说明 `-latest` 是 GitHub 提供的最新稳定镜像，并可能随迁移变化；runner-images 项目建议需要避免迁移时使用明确 OS 标签。镜像内容仍会滚动，因此每次运行还需保存 image、compiler 和工具的精确身份。[GitHub-hosted runners](https://docs.github.com/en/actions/reference/runners/github-hosted-runners) [runner-images](https://github.com/actions/runner-images)

Windows 本机已观察到 MSVC 19.50.35725、CMake 4.1.2-msvc8 与 Ninja 1.12.1；WSL Ubuntu 24.04 已观察到 GCC 13.3.0、CMake 3.28.3 与 Ninja 1.11.1。这些探测只构成候选验证证据。Visual Studio 18 2026 generator 从 CMake 4.2 提供，因此固定 Windows 托管 CI profile 需要 CMake 4.2+，同时不提高项目使用 Ninja 或其他可用 generator 时的 3.20 声明下限。[Visual Studio 18 2026 generator](https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2018%202026.html) [MSVC compiler versions](https://learn.microsoft.com/en-us/cpp/overview/compiler-versions?view=msvc-180)

## Decision

`docs/team/role-assignments.json` 升级为 `gnczmkn.team-roles/2`。每个角色都显式包含 `assignee`、`reviewer` 与 `required`。同一角色的 assignee/reviewer 必须不同；Scientific Authority 与 Architecture Lead 的 assignee 也必须不同，以维持高风险科学/架构签字独立性。空值表示真实空缺；Codex、`codex-*` namespaced execution seat、TBD、unknown 等占位身份不能满足就绪条件。字符串不同只证明结构分离，不能替代真实人员的身份与授权证据。

`docs/governance/toolchain-support-matrix.json` 是 R0 治理证据，不是运行时或发布兼容性 schema。矩阵只设两个 `candidate-primary` profile：

- Windows x64、MSVC `>=19.50.0 <19.60.0`；本机以 Ninja 验证，托管 CI 使用 `windows-2025-vs2026` 与 Visual Studio 18 2026 generator；
- Ubuntu 24.04 x64、GCC `>=13.0.0 <14.0.0`；本机/托管 CI 均使用 Ninja，托管 runner 为 `ubuntu-24.04`。

两条 profile 都要求 C++17、Release、warnings-as-errors、CMake 3.20+、Python 3.8+ 和可运行治理检查的 PowerShell。Windows 托管 profile 因 generator 额外要求 CMake 4.2+。每个 CI run 必须输出 runner OS/architecture/name、image identity、compiler、CMake、generator、PowerShell、Python 与 Git 版本。镜像 family 固定不代表其每周 patch 固定。

CI checkout 固定到 `actions/checkout` v6.0.2 的 commit `de0fac2e4500dabe0009e67214ff5f5447ce83dd`，并登记进 provenance inventory。[checkout v6.0.2](https://github.com/actions/checkout/releases/tag/v6.0.2) [pinned commit](https://github.com/actions/checkout/commit/de0fac2e4500dabe0009e67214ff5f5447ce83dd)

w64devkit/GCC 16.2 只服务冻结 Legacy 复现，保持 `evidence-only` 且不具备产品资格。Clang、macOS、ARM、MinGW 产品构建、sanitizer、动态 package ABI 和 Python wheels 尚未 qualified。

候选 profile 只有在以下三项全部成立后才能改为 supported：本 ADR 由 Product Owner 与 Architecture Lead 接受；八个 required role 都有独立 assignee/reviewer；固定 runner 的新 workflow 成功运行并保存精确环境证据。当前整体状态为 `candidate-not-supported`，R0-GOV-001 最多进入 review。

## Consequences

- Positive: 声明的 CMake 下限与 preset schema 重新一致。
- Positive: 本机探测、托管 runner family、compiler range 和每次 run 的精确身份分别表达，避免把滚动镜像误作 patch 承诺。
- Positive: CI 在 Release 与 warnings-as-errors 下覆盖全量 CTest 和 repository verification。
- Cost: Windows 本机 Ninja 验证与托管 Visual Studio generator 是两条证据路径，出现差异时需要单独归因。
- Cost: Ubuntu 本机 WSL 没有 PowerShell，只能提供 C++/CMake smoke；完整治理 CTest 需由托管 runner 补齐。
- Risk: 固定 runner label 仍会更新预装工具，workflow 必须保留每次运行的 identity 输出。
- Governance blocker: 角色、签字和新 CI run 未闭合，不能宣称已支持或将任务标为 done。

## Alternatives considered

- 提高项目最低 CMake 到 3.25：可以保留 preset schema 6，但 D-003 尚未被有权角色接受，现有字段也不需要新版 schema。
- 继续使用 `*-latest`：配置较短，但 OS 迁移会改变候选平台边界。
- 固定 runner 内每个预装工具 patch：hosted image 持续滚动，此承诺无法由 runner label 保证。
- 把本机验证直接定义为正式支持：缺少角色接受与托管双平台运行证据。
- 把 Legacy w64devkit 用作产品 Windows profile：会跨越 Legacy evidence firewall，且 GCC 16.2 与目标 MSVC profile 不同。

## Verification

- 静态 validator 检查角色 schema、双人评审、科学/架构独立性和占位身份；
- validator 对齐 C++17、CMake 3.20、preset schema 2、两个 compiler range 与所有排除项；
- workflow 检查固定 runner label、checkout commit、Release、warnings-as-errors、CTest、repository verification 和 identity 输出；
- mutation suite 拒绝缺 reviewer、同人评审、无签字 Accepted、preset 漂移、latest runner、浮动 action、无上界 compiler、Legacy 晋升、空角色虚假 readiness 和互不相同的 `codex-r0-*` 虚拟席位；
- 本地 Windows Debug/Release 与 Linux GCC smoke 的命令和结果记录在工作包；新 hosted CI run 在 push 前保持显式 pending。

## Acceptance and supersession

Product Owner 与 Architecture Lead 具名接受后，可将本 ADR 改为 Accepted；角色文件同时必须满足 required slot 和独立性约束。新 workflow 在两个固定 runner 上成功后，另行更新矩阵与报告，将经过证据覆盖的 profile 改为 supported。任何 OS、architecture、compiler family/range、CMake minimum、generator、warning policy、sanitizer、ABI 或发布目标变化都需要重新评审。
