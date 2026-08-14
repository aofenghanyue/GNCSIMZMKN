# Contributing

## 工作流

1. 从 `docs/tasks/backlog.json` 选择当前 gate 内依赖已满足的任务。
2. 明确一个可运行结果和对应的直接失败测试。
3. 建立短分支，推荐 `codex/<task-id>-<topic>` 或 `work/<task-id>-<topic>`。
4. 实现代码、fixture 或 oracle，并同步最小必要文档。
5. 运行相关测试；共享或合并前运行完整仓库检查。
6. 交付说明列出结果、验证、已知限制和下一步。

## 评审重点

- 科学变更：公式、单位、frame、适用域、容差和独立 reference。
- 架构变更：依赖方向、state owner、时间语义、commit 和 effect。
- 代码变更：接口、生命周期、错误处理、测试和诊断。
- 发布变更：许可、外部分发、阶段门和兼容承诺。

局部实现依靠代码与测试评审。公共科学语义、架构边界、许可和阶段门需要仓库所有者或其指定 owner 确认。AI 不通过机器互签取得批准资格。

## 提交信息

推荐格式：

```text
<task-id> <area>: <concise change>
```

示例：

```text
R0-SCI-002 oracle: add minimal 3DoF reference trajectory
```

## 禁止事项

- 将 Legacy 源目录加入 include path 或 CMake target；
- 使用字符串、RTTI、全局 registry 或任意 callback 填补未设计语义；
- 用最终 CSV 相似度替代公式、kernel 和 closure oracle；
- 在阶段门通过前声明后续阶段能力；
- 复制缺少 provenance 的旧模型参数或第三方数据；
- 为单次交付新增角色授权链、验收回执或历史状态镜像。
