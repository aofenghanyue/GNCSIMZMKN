# 开放决策

| ID | 决策 | 截止 gate | 默认约束 | Owner role | 状态 |
| --- | --- | --- | --- | --- | --- |
| D-001 | 最终产品名、namespace 与可执行文件名 | G1 | 工作名 `GNCZMKN Next`，namespace `gnc` | Product Owner | Open |
| D-002 | 仓库和框架许可证 | G1 | 仅内部使用 | Product Owner | Open |
| D-003 | 支持的 OS、编译器与最低版本 | G1 | C++17 + CMake 3.20；Windows/Linux smoke CI | Architecture Lead | Open |
| D-004 | JSON/YAML 和测试库的版本锁；Eigen 已由 ADR-0012 锁定为 3.4.0 | G1 | 除 Eigen 3.4.0 外不加入三方依赖，等待直接 consumer 和 ADR | Architecture Lead | Open |
| D-005 | 稳定 ID、semantic hash 与 canonical encoding | G2 | 仅 fixture 字符串，无稳定实现 | Compiler Lead | Open |
| D-006 | Outcome/Diagnostic 的 C++ 表达与异常边界 | G2 | 目标语义服从 07，语法待 ADR | Architecture Lead | Open |
| D-007 | State block、slot、arena 与 codec 布局 | G3 | Descriptor 语义固定，物理布局待 benchmark | Runtime Lead | Open |
| D-008 | Artifact Store 首版目录、原子提交与 hash | G6 | 本地文件系统最小实现 | Evidence Lead | Open |
| D-009 | 性能、容量和确定性基线 | G3 | 先记录测量，不承诺硬实时 | Runtime Lead | Open |
| D-010 | Python binding 技术与数组所有权 | G7 | Application DTO 优先 | Application Lead | Open |
| D-011 | R8 的实际交付范围 | G8 | PressureOnly，等待真实 consumer | Product Owner | Open |
| D-012 | Legacy archive 对外分享与数据许可 | G1 | 仅授权团队可见 | Product Owner | Open |

任何成员都可以提交 ADR proposal。表中默认约束只能维持现状，不能被当作最终 API 承诺。
