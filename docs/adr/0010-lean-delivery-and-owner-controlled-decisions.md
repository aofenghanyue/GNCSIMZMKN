# ADR-0010: Lean delivery and owner-controlled decisions

- Status: Accepted
- Date: 2026-08-14
- Owner: Repository owner
- Related tasks: R0-GOV-001
- Supersedes: ADR-0009 中的机器 actor 授权与互签规则

## Context

R0 执行逐步把机器角色授权、相互签字、commit/fileset 锁、CI 收据副本、readiness 快照和大量治理 mutation 变成强制交付。此类产物提高了维护成本，也让后续智能体容易把审计活动理解为产品进展。

仓库所有者已停止原有多智能体执行，并要求立即调整为开发结果优先、单一实现智能体的工作方式。现有科学设计、schema、架构边界和自动测试仍有技术价值，需要与机器自我验收链分离。

## Decision

1. 仓库所有者保留产品范围、优先级、科学口径、架构例外、阶段门和发布决定。
2. AI 智能体可以分析、实现、测试、同步必要文档和提出建议。AI 不拥有最终批准、阶段门签署或发布授权。
3. 2026-08-12 登记的四个机器 actor 授权立即撤销。创建多个 agent 不产生独立决策资格。
4. 默认使用一个实现智能体。并行或子智能体只在用户明确要求时启用。
5. 工作优先级为可执行科学或产品切片、直接回归测试、完成切片所需的最小契约与文档。
6. 没有当前 consumer 或已复现回归时，不创建治理 schema、任务验收回执、角色授权链、CI 回执镜像、commit/hash 锁或大批量 mutation。
7. 技术验证继续保留：构建、直接单元测试、科学 reference 对比、schema 实例校验、关键架构依赖和 Legacy 隔离检查。
8. 验证强度与变更风险相称。完整套件用于共享或合并前检查，文档小改只运行直接相关验证。
9. 当前树只保存现行规则和可复用技术事实。旧审计与机器签署记录由 Git 历史承担追溯，不改写既有历史。
10. R0 阶段边界继续有效。R1～R8 生产能力需要仓库所有者明确解锁或正式通过 R0 gate。

## Retained technical decisions

- ADR-0004 的 schema v1 identity、field graph、repository-root evidence locator 和 PlanProofRecord v1 边界继续有效。
- ADR-0005 的九模块物理 DAG、两个逻辑标签和 22 条 Legacy ownership 映射继续有效。
- ADR-0009 的 C++17、CMake presets、Windows/MSVC 与 Ubuntu/GCC CI profile 继续有效。
- 上述基线由源码、ADR 和直接自动测试证明，不再依赖机器 actor disposition 或任务验收收据。

## Consequences

- 后续智能体能够从少量当前文件恢复工作，历史状态不会占据默认上下文。
- 任务完成度回到可执行产物和直接证据，机器互签不再推动状态。
- 需要仓库所有者判断时，智能体提交一个收敛后的问题、推荐项和影响范围。
- 旧分支、stash 和提交保持可恢复；治理重置不会破坏已有技术工作。
