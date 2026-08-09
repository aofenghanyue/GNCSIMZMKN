# ADR-0002: C++17 and CMake modular monolith

- Status: Accepted
- Date: 2026-08-09
- Owners: Architecture Lead
- Related tasks: R0-GOV-001, R1-FND-001
- Architecture references: 00 §6、02 §13、04 §15

## Context

目标蓝图采用实验室级模块化单体，并允许同一构建内使用 C++17 接口、模板和 Eigen view。动态 ABI 和多进程部署仍处于延期状态。

## Decision

Bootstrap 使用 C++17、CMake 3.20+ 和 CTest。逻辑模块先表现为独立 CMake interface target；后续依据编译时间、性能和语言绑定证据决定是否形成 compiled library。

## Consequences

- Windows 和 Linux 可以共享基础构建入口。
- 当前没有稳定二进制 ABI 承诺。
- 新三方依赖和 public ABI 需要独立 ADR。

## Verification

- Debug/Release preset 可配置和构建；
- CLI 与 smoke tests 在 CI 平台运行；
- 编译特性固定为 `cxx_std_17`。
