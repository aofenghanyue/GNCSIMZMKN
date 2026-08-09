# 工作量与人员建议

以下区间用于 staffing 和阶段预算，单位为净工程人月，已排除旧用户迁移与 API 兼容工作。

| 阶段 | 工作范围 | 人月 |
| --- | --- | ---: |
| R0 | 科学基线、目标契约、fixture 和 guards | 3～5 |
| R1 | foundation、contracts、Model SDK | 7～11 |
| R2 | Semantic Compiler、Plan、proof 与 explain | 10～16 |
| R3 | Transactional Session、closure 与 YYZ | 12～20 |
| R4 | Observation、Artifact 与 Evidence | 6～10 |
| R5 | Research Workflow 与工具链 | 7～12 |
| R6 | Experiment、Python 与 RL | 6～10 |
| R7 | Application Control、LLM 与 Blueprint | 8～14 |
| R8 | Snapshot、realtime 与 frontend demo | 8～15 |
| 横向稳定化 | 性能、构建、文档、跨阶段返工 | 8～15 |
| 合计 | R0～R8 | 75～128 |

建议的最小核心团队：

- 1 名 Architecture/Compiler Lead；
- 1 名 Runtime/Numerics Lead；
- 1 名 Model SDK/GNC Lead；
- 1 名 Validation/Evidence Lead；
- Product Owner 与 Scientific Authority 按 gate 参与。

R1 的 contract、algorithm 与 fixture 可以并行；R2→R3→R4 仍有明显串行依赖。增加人员前应先确认工作包拥有独立输入、输出和验收证据。
