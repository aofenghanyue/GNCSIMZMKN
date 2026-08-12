# 团队协作模型

## 1. 角色

| 角色 | 最终责任 |
| --- | --- |
| Product Owner | 范围、优先级、预算与发布判断 |
| Scientific Authority | 数学、物理假设、reference、适用域与科学差异 |
| Architecture Lead | 防火墙、依赖、owner、time、commit 与 ADR |
| Model SDK Lead | contracts、definitions、recipes、algorithms 与 packages |
| Compiler Lead | source、IR、binding、proof、plan 与 explain |
| Runtime/Numerics Lead | Session、state、transaction、integration 与 determinism |
| Evidence/Workflow Lead | observation、artifact、lineage、tasks 与 tool adapters |
| Application Lead | CLI、Python、proposal、control 与 frontend adapters |
| Validation Lead | fixtures、oracles、failure injection、CI 与 gate evidence |

小团队可以让一人承担多个角色。Scientific Authority 与 Architecture Lead 对高风险改动保持独立签字。

责任主体可以是可解析的人类身份，也可以是经过仓库所有者明确授权的机器智能体。机器智能体必须在授权登记中公开身份类型、实际 task/thread binding、角色范围和日期，且不得伪装成人类。未登记别名、泛化工具名称和占位符均不具备决策资格。同一事项的实现者与最终复核者必须来自不同责任主体和不同执行 task；Scientific Authority 与 Architecture Lead 继续保持独立。

## 2. 决策规则

- 目标架构语义变化：Architecture Lead + Scientific Authority + Product Owner。
- 科学模型变化：Scientific Authority + 对应模块负责人。
- 公共 API/schema：Architecture Lead + 至少一个真实 consumer 负责人。
- 三方依赖、许可证和发布：Product Owner + Architecture Lead。
- 普通局部实现：模块负责人 + reviewer。

## 3. 工作节奏

- 每周一次 gate/evidence 评审；
- 每两周一次架构债务和开放 ADR 评审；
- 每个纵向 slice 结束后举行科学差异评审；
- 阶段门只依据已提交 evidence，不依据口头完成度。

## 4. 任务状态

`planned → ready → in_progress → review → done`

`blocked` 可以从任意未完成状态进入。阻塞记录需要说明缺少的输入、责任角色和下一次复查日期。

## 5. 交接要求

负责人离开任务前需要留下：当前 branch/commit、已完成产物、未通过测试、开放决策、复现命令和下一步。自由文本聊天记录不能充当唯一交接材料。
