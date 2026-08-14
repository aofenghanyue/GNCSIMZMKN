# 团队协作模型

## 角色与权限

| 角色 | 最终责任 |
| --- | --- |
| Product Owner | 范围、优先级、预算、阶段门与发布判断 |
| Scientific Authority | 数学、物理假设、reference、适用域与科学差异 |
| Architecture Lead | 依赖、owner、时间、提交语义与 ADR |
| Model SDK Lead | definitions、recipes、algorithms 与 packages |
| Compiler Lead | source、IR、binding、proof、plan 与 explain |
| Runtime/Numerics Lead | Session、state、transaction、integration 与 determinism |
| Evidence/Workflow Lead | observation、artifact、lineage、tasks 与 tool adapters |
| Application Lead | CLI、Python、control 与 frontend adapters |
| Validation Lead | fixtures、oracles、failure injection、CI 与 gate evidence |

小团队可以由同一人承担多个角色。仓库所有者负责产品范围和优先级，并在缺少已指派专业 owner 时保留最终决定。

AI 智能体可以分析、实现、测试、整理证据和提出建议。它不能替代 Product Owner、Scientific Authority、Architecture Lead 或阶段门签署人，也不能通过创建多个 agent 获得独立批准资格。高风险选择需要仓库所有者或其明确指定的人确认。

## 决策规则

- 普通局部实现由当前任务 assignee 完成，并通过自动测试验证。
- 科学公式、frame、单位、时间或容差的公共语义变化需要 Scientific Authority 或仓库所有者确认。
- 模块依赖、state owner、公共 API/schema 和跨进程边界变化需要 Architecture Lead 或仓库所有者确认，并在影响公共语义时提交窄 ADR。
- 三方依赖、许可证、外部分发、阶段门和发布由仓库所有者决定。
- AI 应把待决问题压缩成一个具体选择、影响范围和推荐项，避免生成成套审批材料。

## 工作节奏

1. 从 backlog 选择一个依赖已满足的纵向切片。
2. 先实现最小可执行路径和直接失败测试。
3. 同步必要契约与说明。
4. 运行与风险相称的验证。
5. 交付可运行结果、剩余限制和一个明确下一步。

聊天记录只用于协作。当前任务状态写入 backlog；跨会话恢复信息写入 `r0-execution-state.md`；稳定公共决定写入 ADR。
