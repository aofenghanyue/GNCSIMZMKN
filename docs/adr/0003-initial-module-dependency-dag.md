# ADR-0003: Initial module dependency DAG

- Status: Accepted
- Date: 2026-08-09
- Owners: Architecture Lead
- Related tasks: R0-ARCH-001, R0-ARCH-002
- Architecture references: 02 §13

## Context

Plan、Commit 与 Artifact/Control 三道防火墙需要在代码依赖中尽早可见。空骨架也应阻止方便性的反向 include。

## Decision

初始模块为 foundation、contracts、model_sdk、compiler、kernel、evidence、workflow、application 和 adapters。CMake 依赖遵循：

```text
foundation <- contracts <- model_sdk <- compiler
foundation + contracts <- kernel
foundation + contracts <- evidence <- workflow
compiler + kernel + evidence + workflow <- application <- adapters
```

Kernel 不链接 Compiler。具体 packages 只在 composition root 与 descriptors/linked implementation 边界出现。

## Consequences

- Plan runtime contract 后续可能从 compiler 输出类型中抽到 contracts；
- Application 是组合用例层；
- CLI 只依赖 adapters/Application。

## Verification

- CMake target graph 符合上述方向；
- architecture guard 检查禁止 include；
- 任一反向依赖需要 superseding ADR。
