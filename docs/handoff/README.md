# 新团队交接总览

## 当前结论

仓库处于 R0。C++17/CMake 空骨架、schema、科学约定测试、架构基线和 Legacy 只读参照已经存在；仿真生产能力尚未开始。

2026-08-14 起采用单一实现智能体和可执行交付优先的工作方式。AI 可以实现与测试，最终产品、科学、架构、阶段门和发布决定由仓库所有者或其指定 owner 作出。

## 恢复工作

1. 阅读 [R0 当前执行状态](r0-execution-state.md)。
2. 查看 [任务台账](../tasks/backlog.json)。
3. 阅读当前任务直接引用的 ADR、架构分册和测试。
4. 从一个可执行纵向切片开始。

历史审计、旧机器角色授权、互签回执和 CI 收据副本不属于默认阅读材料。需要追溯时查询 Git 历史。

## 当前硬边界

- `reference/legacy/` 只读且不参与构建。
- R0 结束前不建设 R1～R8 的生产 Compiler、Session、Artifact Store、Python 或前端能力。
- 科学结论需要独立 reference、适用域和 provenance。
- 公共架构或科学选择需要仓库所有者或其指定 owner 确认。
- 任务进展以源码、fixture、oracle 和直接自动测试为准。

## 关键文件

- [项目章程](project-charter.md)
- [Greenfield 边界](greenfield-boundary.md)
- [实现与证据契约](implementation-contract.md)
- [团队协作模型](team-operating-model.md)
- [阶段门](release-gates.md)
- [R0 推荐开发顺序](../tasks/first-wave.md)
- [ADR 索引](../adr/README.md)
