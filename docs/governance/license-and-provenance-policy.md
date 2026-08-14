# 许可证与来源策略

- 状态：R0 interim，等待仓库所有者决定 G1 分发范围
- 决策记录：ADR-0008
- 适用范围：当前仓库内容、架构蓝图、fixture/oracle/benchmark、Legacy reference，以及构建和验证实际调用的外部输入

本文件约束仓库工作流，不提供许可证或法律意见。

## 当前边界

| 范围 | 当前处理 | 外部分发 | 关闭条件 |
| --- | --- | --- | --- |
| 仓库源码、文档、fixture、oracle、benchmark | 当前授权工作区内开发和验证 | blocked | 仓库所有者确认分发范围、权属和许可证路线 |
| `design-notes/` 架构蓝图 | 当前授权工作区内使用 | blocked | 作者/权利来源和可许可范围闭合 |
| `reference/legacy/` | read-only、evidence-only | blocked | 逐项权属和许可证据闭合；任何候选包仍需显式排除或批准 |
| Eigen 与 w64devkit | 仅隔离 Legacy 复现 | not redistributed | 进入产品或分发包时按实际文件/runtime 复核 |
| 宿主 Python、PowerShell、CMake、编译器 | 构建与验证时就地执行 | not redistributed | bundling、容器、runtime 或安装包进入交付时复核 |
| `actions/checkout` | 固定 commit 的 CI 调用 | not redistributed | workflow 依赖变化时同步外部输入记录 |

origin 当前公开可见。该事实由仓库所有者单独处置；本地验证不会把公开可见性推导为分发许可证，也不会尝试改变远端状态。

## 新内容和依赖

提交新的第三方代码、数据、模型、媒体、字体、archive 或 binary 前，至少记录：

1. 实际用途和当前 consumer；
2. 精确上游 URL、version/tag/commit，以及可获得的原始 hash；
3. 原始许可证或数据权限证据及其适用文件/组件；
4. 内容是否进入仓库、静态/动态链接、runtime、生成输出或最终交付包；
5. 必需的 notice、attribution、source offer、relink、使用范围或保留限制；
6. 目标路径与 provenance parent。

信息未闭合时不 vendor、不声明可外发。新产品/公共依赖仍需要窄 ADR。

## 数据和生成内容

- fixture、oracle 与 benchmark 记录 source identity、模型 identity、适用域和生成方式；科学独立性继续由其直接 reference 与测试证明。
- 论文、图表、外部数据和人工转录只保存当前权利允许的内容；受限原文使用合法外部 locator 和可复核 identity。
- 派生内容继承所有未解决的上游分发限制。生成工具本身不自动决定输出的权利状态。
- Legacy 来源只能支撑旧行为事实，不能成为新模型权威或分发许可。

## 外部分发

任何 source、binary、archive、数据集或公开报告候选都需要明确文件范围。候选中每个第三方或受限项必须具有适用于该交付形式的权利结论和所需 notices。Legacy 与 `NOASSERTION` 项默认阻断候选。

当前实现智能体没有 push、release、远端可见性变更或许可证选择授权。仓库所有者作出 G1 范围选择后，再按真实候选补齐后续工作。

## 直接验证

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validate-license-provenance.ps1
```

该命令核对全部 Git 跟踪文件的范围覆盖、需审查 binary/archive、真实 CMake/CI 外部输入、Legacy archive 身份与许可证信号，并运行四个关键失败用例。

显式检查外部分发就绪状态：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validate-license-provenance.ps1 -RequireExternalReady
```

仓库所有者决定和相关权利尚未闭合时，该命令返回退出码 2，并列出当前 blockers。
