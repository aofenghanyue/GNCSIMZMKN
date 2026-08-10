# 新团队交接总览

## 交接结论

本仓库已经具备全新大型框架的启动条件：目标架构、空代码骨架、阶段任务、验证入口和旧项目参照均有明确位置。当前交付只承诺 bootstrap 能力，团队需要从 R0 开始取得科学与架构基线。

## 交接包包含什么

| 内容 | 位置 | 当前状态 |
| --- | --- | --- |
| 目标架构 v1 | `design-notes/gnczmkn-architecture-roadmap/` | 规范设计 |
| Greenfield 实现边界 | `docs/handoff/greenfield-boundary.md` | 已接受 |
| 可构建 C++17/CMake 骨架 | `framework/`、`apps/`、`tests/` | 已验证 bootstrap |
| 阶段任务与依赖 | `docs/tasks/backlog.json` | R0 可领取 |
| ADR 体系 | `docs/adr/` | 已启用 |
| 架构机器基线 | `docs/architecture/` | R0 review |
| Fixture 与 oracle 插槽 | `fixtures/`、`oracles/` | 待 R0 填充 |
| 旧项目只读快照 | `reference/legacy/` | 行为与科学参照 |
| 仓库自动检查 | `tools/verify-repository.ps1` | bootstrap gate |

## 新团队第一天

1. 由仓库所有者填写 `docs/team/role-assignments.json`。
2. Architecture Lead 主持 90 分钟架构主线走查。
3. Scientific Authority 确认四元数、坐标、单位、时间和 YYZ/CAVH reference 的权威来源。
4. Validation Lead 运行 `tools/bootstrap.ps1` 并保存输出。
5. 团队只领取 `docs/tasks/first-wave.md` 中状态为 `ready` 的任务。

## 当前硬边界

- Legacy archive 不参与构建。
- R0 结束前不建设产品级 Compiler、Session、Artifact Store、Python 或前端。
- 任何科学结论都需要独立 reference 或明确 provenance。
- 任何架构矛盾都通过 ADR 和阶段门处理，局部实现不得隐藏选择。

## 关键阅读

- [项目章程](project-charter.md)
- [Greenfield 边界](greenfield-boundary.md)
- [架构阅读路线](architecture-reading-guide.md)
- [实现与证据契约](implementation-contract.md)
- [团队协作模型](team-operating-model.md)
- [阶段门](release-gates.md)
- [工作量与人员建议](effort-and-staffing.md)
- [风险登记](risk-register.md)
- [开放决策](open-decisions.md)
- [R0 权威解锁包](r0-authority-unblock-packet.md)
- [首月安排](first-30-days.md)
