# AGENTS.md

本仓库是 GNCZMKN 大型目标架构的全新实现。当前产品状态为空骨架，工作必须服从阶段门，不能把目标文档中的 `V1` 当作已交付能力。

## 开始工作前

依次阅读：

1. `README.md`
2. `docs/handoff/README.md`
3. `docs/handoff/project-charter.md`
4. `docs/handoff/greenfield-boundary.md`
5. `docs/tasks/first-wave.md`
6. 当前任务引用的目标架构分册与 ADR

## 当前阶段

- 当前 gate 为 R0。
- 未通过 R0-GATE 前，不实现 R1～R8 的生产能力。
- 可以为后续阶段编写问题说明、ADR proposal 和测试设计；不得提前建立半成品 manager、callback、plugin runtime 或兼容层。

## 核心边界

- `reference/legacy/` 只读且不参与构建。
- 生产代码不得 include、链接、运行或复制 legacy runtime API。
- 可迁入的科学公式、参数和资产必须附 provenance、适用域、独立 oracle 和新 model identity。
- Compiler 不依赖具体 package 实现。
- Kernel 不依赖 Compiler、领域 package、文件格式、Workflow 或前端。
- Workflow 不读取 Session 内部 state、CycleFrame 或 Runtime Cell。
- Adapter 只能通过 Application API、Artifact 或已批准的稳定 contract 接入。
- 每个 mutable state 有唯一 owner；跨 owner 数据拥有 typed contract 与明确时间关系。

## 实现纪律

- C++ 标准为 C++17；构建入口为 CMake presets。
- 新三方依赖、新公共 schema、新 hash/codec、新线程模型和跨进程边界需要 ADR。
- 先写失败路径和验收证据，再把任务状态从 `planned` 改为 `in_progress`。
- 一个 PR 聚焦一个可审查的纵向切片；机械搬迁与语义修改分开。
- 目标契约只有在源码和自动测试通过后才能标记 `Implemented`。
- 新中文文档与注释使用 UTF-8，并禁用“先否定前项、再转折肯定后项”的对照句式。

## 验证

最小检查：

```powershell
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
powershell -NoProfile -ExecutionPolicy Bypass -File tools/verify-repository.ps1
```

修改架构依赖、状态语义、时间语义或 legacy 边界时，必须同步增加 repository guard。

## 任务管理

- 唯一任务源是 `docs/tasks/backlog.json`。
- 状态值：`planned`、`ready`、`in_progress`、`blocked`、`review`、`done`。
- `done` 需要 deliverables、acceptance 和 evidence 全部闭合。
- 领取任务时填写 `assignee`；架构或科学裁决仍由对应 `owner_role` 审批。
- 发现蓝图矛盾时先登记 ADR/issue，不能在局部代码中静默选择。
