# 风险登记

| ID | 风险 | 早期信号 | 当前响应 | Owner role |
| --- | --- | --- | --- | --- |
| RK-001 | 术语和对象数量失控 | 同义类型、重复 enum、跨册不同字段 | glossary + schema owner + conformance check | Architecture Lead |
| RK-002 | Legacy 渗入新内核 | include/link 旧路径、wrapper 增长 | dependency guard；只读 archive | Architecture Lead |
| RK-003 | 科学基线缺失 | 只能比较最终 CSV | R0 多层 oracle；G1 阻断 R1 | Scientific Authority |
| RK-004 | Compiler 退化成 parser | 运行期发现绑定、rate、owner | PlanProofRecord、negative compile suite | Compiler Lead |
| RK-005 | Transaction 复杂度失控 | component 继续原地写 state | fixture owner + full replacement delta | Runtime Lead |
| RK-006 | 模型壳与算法继续耦合 | kernel 读取 JSON、logger 或 Session | algorithm six-piece conformance | Model SDK Lead |
| RK-007 | Evidence 平台提前膨胀 | R3 前建设通用 Artifact backend | gate 约束；先最小 local store | Evidence Lead |
| RK-008 | 前端反向定义内核 API | UI 读取 Runtime Cell pointer | Application API + DTO + snapshot | Application Lead |
| RK-009 | 性能目标过晚 | R3 后才发现 layout/arena 不可承载 | R0/R2 建立容量与 benchmark 基线 | Runtime Lead |
| RK-010 | 单点科学知识 | 约定只存在于负责人记忆 | ADR、oracle、formula bundle 和 reviewer | Product Owner |
| RK-011 | 许可证阻断交接 | 资产来源和发布范围不清 | R0-GOV-002/003 | Product Owner |
| RK-012 | 阶段并行过度 | 多个纵向 slice 同时半完成 | YYZ 主 slice 优先；gate 限流 | Architecture Lead |

每个 gate 评审更新概率、影响和处置状态。新增风险必须关联至少一个 backlog task 或明确接受人。
