# ADR-0002: C++17 and CMake modular monolith

- Status: Accepted
- Date: 2026-08-09
- Owners: Architecture Lead
- Decision reviewers: Product Owner, Architecture Lead
- Related tasks: R0-GOV-001, R1-FND-001
- Architecture references: 00 §6、02 §13、04 §15

## Context

目标蓝图采用实验室级模块化单体，并允许同一构建内使用 C++17 接口、模板和 Eigen view。动态 ABI 和多进程部署仍处于延期状态。

## Decision

Bootstrap 使用 C++17、CMake 3.20+ 和 CTest。逻辑模块先表现为独立 CMake interface target；后续依据编译时间、性能和语言绑定证据决定是否形成 compiled library。

R0 支持边界固定如下：

| Lane | Platform and compiler policy | Build entry | Gate meaning |
| --- | --- | --- | --- |
| Local reference | Windows；MSYS2 MinGW-w64 GCC 15.1.0 | CMake 4.0.3、Ninja 1.13.0、`dev` 与 `release` presets | 已记录的可复现参考环境 |
| Required CI | GitHub-hosted `windows-latest`；runner 默认 C++ compiler | configure、build、CTest、repository checks | 每次变更必须通过 |
| Required CI | GitHub-hosted `ubuntu-latest`；runner 默认 C++ compiler | configure、build、CTest、repository checks | 每次变更必须通过 |

语言契约为 C++17，最低构建系统契约为 CMake 3.20。Repository presets 使用 Ninja；CI 可以使用平台默认生成器。每次 CI 配置日志记录实际 compiler identity 和版本，hosted image 更新后的首个通过结果构成新的环境证据。矩阵之外的平台与编译器属于未验证环境，新增正式支持 lane 需要可复现的 configure、build、test 和 repository-check 证据。

源码兼容性是当前支持承诺。稳定二进制 ABI、动态 package ABI、多进程部署和跨工具链产物兼容继续延期，并分别服从后续 ADR 与阶段门。

## Consequences

- Windows 和 Linux 可以共享基础构建入口。
- 当前没有稳定二进制 ABI 承诺。
- 新三方依赖和 public ABI 需要独立 ADR。
- `dev` 用于快速反馈；`release` 启用 warnings-as-errors 并承担更严格的编译门禁。

## Verification

- `dev` 与 `release` preset 均可配置、构建并通过 CTest；
- CLI、smoke tests 和 repository checks 在两个 required CI lane 运行；
- 编译特性固定为 `cxx_std_17`；
- R0 本地参考环境与结果记录在 `docs/quality/gate-decisions/B0-2026-08-09-passed.md`。
