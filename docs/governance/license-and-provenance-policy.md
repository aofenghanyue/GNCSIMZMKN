# 许可证与来源策略

- Policy ID：`GNC-LIC-PROV-001`
- 状态：Proposed / enforced as an interim fail-closed safeguard
- 决策记录：`ADR-0008`
- Owner roles：Product Owner、Architecture Lead
- 适用范围：仓库代码与文档、数据/模型/媒体、fixture/oracle、archive、生成 Artifact、外部依赖与工具

## 1. 非授权声明

本策略约束项目工作流，不是许可证或法律意见。仓库存在、远端可访问、内部可读取、上游可下载、构建成功或报告已生成，都不能单独证明复制、修改、再分发或发布权限。

在 Accepted ADR 选定仓库许可证前：项目空间内仅按现有访问授权处理；所有新的外部分享、push 到公共远端、release、附件发送、公开报告、数据集、容器、二进制和 archive 均关闭。

## 2. 状态模型

| 字段 | 允许值 | 含义 |
| --- | --- | --- |
| `license.scan_result` | `no-license-information-found` | 在定义的 scan scope 内未发现许可证信息；对应检测值 `NONE`，不代表许可 |
|  | `license-information-present` | 找到上游许可证或声明，仍需确认逐文件/组件范围和兼容性 |
|  | `not-scanned-nonredistributed-tool` | 工具只在外部环境执行，尚未扫描其分发包 |
| `license.concluded_expression` | SPDX expression | 已基于证据得到明确结论 |
|  | `NOASSERTION` | 尚未或无法得到结论；当前 inventory 全部使用此值 |
| `internal_handling` | `existing-access-only` | 仅在既有授权的项目空间处理，保留来源、分类和 lineage |
|  | `evidence-only` | 只允许验证、比对与审查，不链接产品、不修改、不分享 |
|  | `subject-to-upstream-terms` | 使用外部工具/依赖须遵守其上游条款 |
| `external_distribution` | `blocked-pending-accepted-adr` | 等待仓库许可证和双角色批准 |
|  | `blocked-pending-item-review` | 单项权属/许可证/数据权限未闭合 |
|  | `not-redistributed` | 仅执行外部安装，不进入项目分发包 |

`NONE` 和 `NOASSERTION` 永远不能映射为 `allowed`。`LicenseRef-*` 必须指向仓库中保存的完整自定义许可证文本，并登记版权方、适用 scope 和 hash。

## 3. 当前材料矩阵

| 材料 | 内部处理 | 外部分享 | 当前要求 |
| --- | --- | --- | --- |
| 新仓库代码、文档与蓝图 | `existing-access-only` | blocked | 维持 git identity；等待 owner/贡献权属与仓库许可证决定 |
| fixture、oracle、验证数据和报告 | `existing-access-only` | blocked | 登记输入、生成器、hash、分类和 parent lineage |
| 冻结 Legacy archive | `evidence-only` | blocked | 只读、hash 固定、无产品依赖；任何分享先补齐权属/许可证 |
| Eigen 3.4.0 | `subject-to-upstream-terms` | `not-redistributed` | 精确 archive/hash；复核实际 include 文件、MPL/BSD/LGPL scope 与义务 |
| w64devkit 2.9.1 | `subject-to-upstream-terms` | `not-redistributed` | 按捆绑组件/runtime 处理，禁止使用顶层源码许可证概括整个 kit |
| MSVC/CMake/Ninja/Python/PowerShell/Git | `subject-to-upstream-terms` | `not-redistributed` | 记录可执行 identity/version/environment；发布时重新审查 bundling |

## 4. 新内容准入

在下载、复制、生成后提交或配置为自动获取第三方内容之前，inventory record 必须包含：

1. stable item id、category、scope、purpose 和 accountable owner；
2. 原始发布者与主来源 URL，精确 version/tag/commit/date；
3. 原始 archive 或文件 SHA-256、字节数和获取方式；
4. copyright holder、许可证文本、SPDX expression 与逐文件/组件适用范围；
5. 修改、patch、静态/动态链接、代码生成、训练/推理、数据转换或工具执行方式；
6. compatibility conclusion、attribution、NOTICE、source offer、relink、share-alike、专利、商标与导出义务；
7. 数据 consent/permission、隐私、保密、敏感性、地域、用途和保留期；
8. 目标 repository path、产品依赖层、Artifact 类型和 downstream consumers；
9. reviewer、日期、决定、例外有效期与不可变 evidence refs。

信息缺失时保持 `NOASSERTION` 并拒绝 vendoring 和外部分发。URL 或包管理器名称不能替代保存原始许可证证据；转存 mirror 时同时保留 upstream identity。

## 5. 外部工具和生成内容

外部工具即使不提交，也必须在可复现实验中记录 executable/product identity、version、来源、archive hash（可获得时）、调用参数、工作目录、locale、受控环境变量、license mode 与输出 hash。w64devkit 等工具包逐组件处理；编译器 runtime、模板、静态库、代码生成输出和嵌入资源需要单独判断是否进入 Artifact。

生成内容不因“由工具生成”自动成为项目可分发材料。Artifact 必须引用所有 code/data/model/tool parent，并继承其中最严格的许可证、数据权限、分类、保密和导出限制。人工修订登记为新的 provenance event。

## 6. 外部导出门禁

每次导出以明确 manifest 为单位，不接受“整个目录应该没问题”的判断。以下条件全部满足后才能由 Product Owner 与 Architecture Lead 批准：

- 每个文件都有 owner/source/license conclusion，且 scope 与目标使用匹配；
- 仓库许可证、第三方兼容性、NOTICE/attribution/source offer/relink 等义务已物化进导出包；
- 数据/模型/媒体的 consent、用途、受众、地域、保留与删除规则已闭合；
- secrets、PII、客户/项目标识、绝对路径、内部 URL、token、日志和受限技术信息扫描通过；
- Artifact lineage、input/output hash、构建/生成环境、SBOM/dependency lock 与重现说明齐全；
- Legacy、`NOASSERTION`、过期 exception 和未审查 `LicenseRef-*` 项为零；
- approval record 与 export receipt 固定 approver、commit、manifest hash、destination、audience、time 和撤回/通知路径。

导出后发现错误时停止继续分发，保留 evidence，通知 Product Owner/Architecture Lead，并根据 destination 执行撤回、替换、notice 或其他处置。

## 7. 变更与例外

例外必须有 owner、scope、理由、风险、补偿控制、到期日和 reviewer，不得把 `NOASSERTION` 临时改写为许可证。策略、inventory 或 checker 被 runtime/public consumer 采用前，需要独立 schema ADR 与兼容性计划。
