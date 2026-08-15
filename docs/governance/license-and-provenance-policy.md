# 许可证与来源策略

- 状态：R0 accepted，G1 public GitHub collaboration / platform rights only
- 决策记录：ADR-0008
- 适用范围：当前仓库内容、架构蓝图、fixture/oracle/benchmark、Legacy reference，以及构建和验证实际调用的外部输入

本文件约束仓库工作流，不提供许可证或法律意见。

## 当前边界

| 范围 | 当前处理 | GitHub public collaboration | release / 平台外分发 |
| --- | --- | --- | --- |
| 仓库源码、文档、fixture、oracle、benchmark | 本地开发、验证及 public origin 协同 | allowed under GitHub platform terms | blocked；需仓库所有者选择许可并确认候选权属 |
| `design-notes/` 架构蓝图 | public origin 中的现有架构输入 | owner-accepted existing exposure | blocked；作者/权利来源和可许可范围仍需闭合 |
| `reference/legacy/` | read-only、evidence-only，当前随仓库公开 | owner-accepted existing exposure | blocked；逐项权属和许可证据闭合后仍需单独决定 |
| Eigen 与 w64devkit | 仅隔离 Legacy 复现 | not bundled | 进入产品或分发包时按实际文件/runtime 复核 |
| 宿主 Python、PowerShell、CMake、编译器 | 构建与验证时就地执行 | not bundled | bundling、容器、runtime 或安装包进入交付时复核 |
| `actions/checkout` | 固定 commit 的 CI 调用 | workflow reference only | workflow 依赖变化时同步外部输入记录 |

origin 保持 public，用于与 `zbyandmoon` 通过 fork、issue、review 和 pull request 协同；`zbyandmoon/GNCSIMZMKN` 是已存在的公开 fork。GitHub 平台条款允许 public repository 的平台内查看和 fork 等功能。仓库当前没有通用分发许可证，public visibility 也不表示已完成 repository content、蓝图或 Legacy 的权利清结论。实现智能体不会自行改变远端状态，也不会在缺少明确授权时 push、merge、tag 或发布 release。

本策略中的 `external_distribution: blocked` 只描述 public GitHub collaboration 之外的主动交付渠道，例如 release、源码或二进制 bundle、数据集、报告附件和 Legacy archive。该状态不会隐藏或否认 public origin 与已存在 fork 的实际公开访问。

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

当前 G1 允许 public GitHub collaboration。实现智能体可以创建本地提交；push、merge、tag、release、公开附件和其他远端变更都需要用户明确授权。

仓库所有者未来若增加 release 或平台外分发，需要先明确文件范围和交付形式。候选中每个第三方或受限项都需要具有适用于该交付形式的权利结论和所需 notices。Legacy 与 `NOASSERTION` 项默认阻断候选。实际合并第三方贡献并把它纳入未来许可或 release 候选时，需要确认贡献者对相应内容的权利；当前协同范围不新增 CLA、DCO 或验收回执。

## 直接验证

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validate-license-provenance.ps1
```

该命令核对已接受的 public GitHub collaboration 范围、全部 Git 跟踪文件的范围覆盖、需审查 binary/archive、真实 CMake/CI 外部输入、Legacy archive 身份与许可证信号，并运行四个关键失败用例。

显式检查外部分发就绪状态：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validate-license-provenance.ps1 -RequireExternalReady
```

已接受的 platform-rights-only G1 范围会使该命令返回退出码 2。这个返回值只阻断 release 和平台外分发，不阻断当前 GitHub 协同。未来只有仓库所有者修改范围并闭合候选权利后，该检查才允许通过。
