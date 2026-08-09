# ADR-0001: Greenfield implementation and legacy reference

- Status: Accepted
- Date: 2026-08-09
- Owners: Product Owner, Architecture Lead, Scientific Authority
- Related tasks: R0-LEG-001, R0-LEG-002
- Architecture references: roadmap governance §1–§7

## Context

目标架构将重写 Compiler、Session、状态、观测和工作流。旧实现仍包含尚未完全物化为独立 oracle 的科学与时间语义。

## Decision

新框架使用独立源码树、构建图和 API。旧仓库以带 commit 和 SHA-256 的只读归档保存在 `reference/legacy/`，只用于 evidence extraction、差异比较和 provenance。生产 target 不获得 legacy include、link 或 runtime dependency。

## Consequences

- 新实现可以遵循目标对象模型，不承担兼容层。
- R0 必须先提取旧行为和科学 reference。
- 新旧对照通过 fixture、文件和报告完成。
- 团队需要维护 legacy provenance 和删除/独立性守卫。

## Verification

- CMake target 不引用 `reference/legacy`；
- repository guard 扫描生产 include；
- legacy archive hash 可验证；
- G6 证明新 runner 只依赖新 Compiler/Session。
