# ADR-0009: Accountable ownership and supported toolchain

- Status: Accepted; machine-agent provisions superseded by ADR-0010
- Date: 2026-08-10
- Amended: 2026-08-14
- Owner: Repository owner
- Related tasks: R0-GOV-001
- Architecture references: 00 §2、team operating model、D-003

## Context

D-003 的默认约束是 C++17、CMake 3.20 和 Windows/Linux smoke CI。项目声明 CMake 3.20+，因此 `CMakePresets.json` 使用 schema version 2。Hosted runner image 会持续更新，workflow 需要固定 image family，并在每次运行中输出工具与编译器身份。

2026-08-12 曾把四个机器智能体登记为 Product、Architecture、Science 和 Validation 责任主体。该方式引入了角色互签、授权验证和大量审计产物。ADR-0010 已撤销这部分授权，并把最终决定交回仓库所有者或其明确指定的人。

## Decision

### Ownership

- `docs/team/role-assignments.json` 登记角色名称和当前实际指派。
- 仓库所有者保留产品范围、阶段门、许可和发布决定。
- AI 可以承担实现与测试，不能进行最终科学、架构或阶段门批准。
- 角色空缺不会触发虚构的机器席位。确需 owner 决定时，任务保留待决状态并提交最小问题。

### Toolchain

仓库支持两个主要 CI profile：

- Windows x64、MSVC `>=19.50.0 <19.60.0`，runner 为 `windows-2025-vs2026`；
- Ubuntu 24.04 x64、GCC `>=13.0.0 <14.0.0`，runner 为 `ubuntu-24.04`。

两个 profile 都使用 C++17、Release、warnings-as-errors、CMake 3.20+、Python 3.8+ 和 PowerShell。Windows Hosted CI 使用 Visual Studio 18 2026 generator，因此该 profile 的 CI 环境需要 CMake 4.2+。

CI checkout 固定为 `actions/checkout@de0fac2e4500dabe0009e67214ff5f5447ce83dd`。workflow 输出 runner、image、compiler、CMake、generator、PowerShell、Python 和 Git 身份。CI 的运行页面与日志由 GitHub 保存；仓库不复制每次运行的完整收据。

w64devkit/GCC 16.2 只服务冻结 Legacy 复现，保持 `evidence-only`。Clang、macOS、ARM、MinGW 产品构建、sanitizer、动态 package ABI 和 Python wheels 尚未纳入支持范围。

## Consequences

- CMake 声明下限与 preset schema 保持一致。
- 固定 runner family 提供稳定入口，每次运行日志保留精确环境信息。
- 工具链资格由 workflow、构建和测试结果直接证明。
- 角色文件不再驱动机器互签或任务验收门禁。
- 新平台、编译器范围或公共构建依赖需要修订本 ADR 或提交后继 ADR。

## References

- [CMake 3.20 presets](https://cmake.org/cmake/help/v3.20/manual/cmake-presets.7.html)
- [GitHub-hosted runners](https://docs.github.com/en/actions/reference/runners/github-hosted-runners)
- [Visual Studio 18 2026 generator](https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2018%202026.html)
- [MSVC compiler versions](https://learn.microsoft.com/en-us/cpp/overview/compiler-versions?view=msvc-180)
