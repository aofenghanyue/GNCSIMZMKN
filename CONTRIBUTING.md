# Contributing

## 工作流

1. 从 `docs/tasks/backlog.json` 选择状态为 `ready` 的任务。
2. 核对依赖、owner role、架构引用和阶段门。
3. 建立短分支，推荐 `codex/<task-id>-<topic>` 或 `work/<task-id>-<topic>`。
4. 先提交契约、测试或 fixture，再提交实现。
5. 运行构建、CTest 和仓库验证脚本。
6. PR 说明必须列出输入、权威输出、失败路径、证据、保持零修改的模块和删除项。

## 评审要求

- 架构评审：依赖方向、owner、时间、commit、effect 和 evidence。
- 科学评审：公式、单位、frame、适用域、数值策略、容差和 reference。
- 代码评审：接口大小、生命周期、异常安全、测试和诊断。
- 交付评审：任务 acceptance、Artifact/日志和阶段门状态。

科学语义变化至少需要 Scientific Authority 批准；架构不变量变化至少需要 Architecture Lead 批准并提交 ADR。

## 提交信息

推荐格式：

```text
<task-id> <area>: <concise change>
```

示例：

```text
R0-SCI-001 foundation: freeze quaternion convention fixtures
```

## 禁止事项

- 将 legacy 源目录加入 include path 或 CMake target；
- 使用字符串、RTTI、全局 registry 或任意 callback 填补尚未设计的语义；
- 用最终 CSV 相似度替代公式、kernel 和 closure oracle；
- 在未通过 gate 时把后续阶段原型宣称为稳定能力；
- 在没有 provenance 的情况下复制旧模型参数或第三方数据。
