# GNCZMKN Next

这是 GNCZMKN 大型目标架构的 greenfield 实现仓库。仓库当前处于 `0.0.0-bootstrap`：只提供可构建空骨架、目标架构、遗留参照、任务系统和阶段门，尚未实现仿真能力。

## 当前交付状态

- 当前 gate：`R0`；Bootstrap 门禁已经通过。
- 允许开展：R0 科学基线、术语与契约、架构守卫、reference fixture。
- 暂缓开展：R1～R8 产品实现；对应任务在 R0 gate 通过后逐阶段解锁。
- 旧 GNCZMKN 只作为只读行为与科学参照，不进入任何生产 target、include path 或运行依赖。

## 新成员从这里开始

1. 阅读 [交接总览](docs/handoff/README.md)。
2. 阅读 [项目章程](docs/handoff/project-charter.md) 与 [greenfield 边界](docs/handoff/greenfield-boundary.md)。
3. 按 [架构阅读路线](docs/handoff/architecture-reading-guide.md) 阅读目标蓝图。
4. 查看 [首轮任务](docs/tasks/first-wave.md) 与机器可读的 [任务台账](docs/tasks/backlog.json)。
5. 领取任务前阅读 [协作规则](CONTRIBUTING.md) 和对应 ADR。

## 构建空骨架

R0 验证需要 CMake 3.20+、Ninja、支持 C++17 的编译器和 Python 3.10+；
Python 仅用于独立科学 oracle 与 schema conformance 工具。

```powershell
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
powershell -NoProfile -ExecutionPolicy Bypass -File tools/verify-repository.ps1
```

也可以运行一键检查：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/bootstrap.ps1
```

当前 CLI 只证明 composition root、编译器和测试工具链可工作：

```powershell
build/dev/gnc_sim --version
build/dev/gnc_sim --self-check
```

## 仓库地图

```text
framework/include/gnc/
  foundation/      数学、数值和值工具
  contracts/       领域、时间、诊断和产物契约
  model_sdk/       definition、recipe、behavior 与 typed view
  compiler/        source、catalog、IR、proof 与 lowering
  kernel/          session、region、state、transaction 与 backend
  evidence/        observation、artifact 与 lineage
  workflow/        experiment、task graph 与 tool port
  application/     use case、control 与 DTO

packages/          可复用领域、模型和工作流贡献
adapters/          CLI、Python、工具、存储、IPC 与前端适配
user/              项目私有研究代码、配置和资产
design-notes/      目标架构蓝图
fixtures/          可执行 reference fixture
oracles/           独立科学与行为判据
reference/legacy/  只读旧仓库快照
docs/tasks/        工作包、依赖和阶段门
```

## 权威顺序

发生冲突时按以下顺序处理：

1. 已接受 ADR 对本仓库的窄实现决策；
2. `design-notes/gnczmkn-architecture-roadmap/` 的目标语义与架构不变量；
3. `specs/` 中已标记 stable 的机器契约；
4. 已通过的 executable fixture、oracle 和自动测试；
5. `docs/handoff/` 的协作与交付规则；
6. `reference/legacy/` 中的旧行为证据。

旧实现出现差异时，需要按缺陷修复、约定统一、显式模型变化、时间语义澄清、浮点差异或无法解释进行分类。无法解释的差异会阻断阶段门。
