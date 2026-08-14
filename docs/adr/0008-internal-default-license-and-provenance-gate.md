# ADR-0008: Internal-default license and provenance gate

- Status: Proposed
- Date: 2026-08-10
- Owners: Product Owner, Architecture Lead
- Related tasks: R0-GOV-002
- Architecture references: 03 §4、05 PackageManifest、08 §11–§12、08 §17、08 §21

## Context

仓库根目录没有分发许可证，冻结 Legacy archive 内也未发现 license/copying/notice 文件或标准许可证声明。GitHub 说明在没有许可证时默认版权规则仍然适用，仓库的公开可见性本身不提供通用复制、修改或分发许可；该说明同时明确不构成法律意见。[GitHub licensing a repository](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository)

当前 R0 已产生源码、架构蓝图、fixture、oracle、Legacy archive 与验证报告，并在隔离复现中使用 Eigen 3.4.0 和 w64devkit 2.9.1。Eigen 3.4.0 上游声明其内容主要采用 MPL-2.0，部分文件采用 BSD 或 LGPL，实际 include 范围仍需逐文件复核。[Eigen 3.4.0 COPYING.README](https://gitlab.com/libeigen/eigen/-/raw/3.4.0/COPYING.README) w64devkit 顶层源码有自己的许可证，同时发布包捆绑 GCC、MinGW-w64、CMake、Ninja 等多项组件，并明确提示 runtime 的许可证义务，不能以一个表达式概括整个工具包。[w64devkit v2.9.1](https://github.com/skeeto/w64devkit/releases/tag/v2.9.1)

R0-GOV-002 需要先建立 fail-closed 治理和可审计 inventory。最终仓库许可证会改变外部权利，按团队规则必须由 Product Owner 与 Architecture Lead 共同决定。仓库所有者已通过 `docs/governance/r0-owner-authorization.json` 授权 `r0-po-agent` 与 `r0-architecture-agent` 承担本轮对应角色；该机器身份授权不包含权属、许可证或外部分发授权。本 ADR 的技术候选不选择 MIT、Apache-2.0、专有许可或其他分发方案。

## Decision

本 ADR 的候选决定是接受 `internal-default / external-blocked` 治理状态，同时保持仓库分发许可证未选择。接受本 ADR 只确认 fail-closed 工作流；选择许可证仍需要另行形成明确的仓库分发许可证决定，并附权属、贡献边界、第三方兼容性和目标发布方式证据。该治理状态不授予或撤销任何法定权利，也不改变上游 GitHub 仓库的既有可见性。

`docs/governance/provenance-inventory.json` 是 R0 治理证据，maturity 为 `governance-evidence-no-runtime-consumer`。每个代码、数据、archive、生成物或外部工具类别至少登记：稳定 identity、scope、owner role、purpose、精确来源、version/date、完整性策略、仓库承载方式、内部处理状态、外部分发状态、分类、许可证扫描结果、许可证结论、证据和 lineage parent。它不是 runtime 或公开 PackageManifest schema。

许可证表达使用 SPDX 语义。已得出结论时使用 SPDX expression；自定义许可证只有在保存了完整许可证文本和稳定 `LicenseRef-*` 标识后才能使用。扫描未发现许可证信息记录为 `NONE`，无法或尚未得出许可证结论记录为 `NOASSERTION`。两者均不表示允许使用或分发。[SPDX license expressions](https://spdx.github.io/spdx-spec/v3.0.1/annexes/spdx-license-expressions/) [SPDX file information](https://spdx.github.io/spdx-spec/v2.2.2/file-information/)

当前所有 inventory 项的 concluded expression 为 `NOASSERTION`。技术候选仍保留 `blocked-pending-accepted-adr`，用于描述 ADR 处于 Proposed 时的冻结输入；最终接受切片会将 repository、blueprint 与 research evidence 原子迁移到 `blocked-pending-rights-and-license-decision`。Legacy archive 保持 `blocked-pending-item-review` 与 evidence-only；Eigen、w64devkit、宿主验证工具链和固定 CI action 保持 `not-redistributed`。工具可执行或依赖可下载不等于项目可以重新打包它们。

逐 artifact 的科学适用域与独立性由 `gnczmkn.scientific-context/1` 治理 sidecar 承载。首个实例绑定 `REF-SCIENTIFIC-CONVENTIONS-001` 的五个事实、来源 raw hash、units/frames/time 适用域、两条参考实现 lane、比较输入和权利 lineage。它明确区分 implementation independence 与 scientific source independence；当前双实现只声明前者已确认，科学来源独立性保持 `not-claimed`。sidecar 的 maturity 为 `governance-evidence-no-runtime-consumer`，不修改三份已接受 Fixture/Oracle/PlanProof v1 schema，也不进入产品 runtime、公共 schema 或 PackageManifest。

引入第三方内容前必须保存精确上游 URL、版本或日期、原始 archive/hash、版权与许可证文本、逐文件或逐组件范围、用途、目标依赖层、修改情况、兼容性判断、notice/source/attribution 义务、数据权限与分类。无法编辑的第三方文件可用外部 annotation 映射，但 annotation 不能创造许可证；REUSE 的逐文件 SPDX 方法作为后续实现候选。[REUSE Specification](https://reuse.software/spec/)

外部导出必须 fail closed。Product Owner 与 Architecture Lead 批准前，导出计划需要闭合 ownership/permission、第三方兼容性、notice/source offer、数据授权、敏感信息、保密与访问范围、完整性、lineage、目标受众和保留期，并生成不可变 approval/export receipt。Artifact 和报告继承所有上游 code/data/tool 的许可证、分类和限制；缺失 parent provenance 时不得晋升或导出。

冻结 Legacy archive 不进入产品依赖、不修改、不公开再分发。其来源 commit、archive hash、字节数、ZIP entry 数、license signal scan 和 evidence-only 状态由静态门禁复核。若后续获得所有权或许可证据，需新审查记录，不能直接修改当前结论。

## Consequences

- Positive: 当前缺少授权和复杂第三方范围时，发布路径保持明确关闭。
- Positive: code、asset、archive、generated artifact 与 external tool 使用同一 provenance vocabulary，并可沿 lineage 传播。
- Positive: `NONE`、`NOASSERTION`、SPDX expression 与 `LicenseRef-*` 的含义可由工具检查。
- Costs: 每个新增依赖、数据源和导出包都需要登记、证据保存与双角色审批。
- Costs: 原始仓库已在 GitHub 可获取，该既有状态需要所有者单独审查；本 ADR 只阻止项目继续产生未经批准的外部分发行为。
- Risks: 本阶段的 scan 只能证明未发现指定信号，不能证明作品没有版权、限制或外部来源。
- Risks: 本 ADR 是工程治理记录，不替代适用司法辖区的法律审查。
- Modules kept unchanged: `framework/`、`packages/`、`adapters/`、`apps/`、`user/` 和冻结 `reference/legacy/`。

## Alternatives considered

- 立即加入 MIT 或 Apache-2.0：可以简化开源分发，但当前没有 owner/贡献权属证据和有权角色批准。
- 立即加入自定义“研究用途”许可证：会产生实质分发条款，且兼容性、定义与执行成本尚未审查。
- 仅保留自由文本提醒：无法阻止 `NOASSERTION` 被误标为可分发，也无法验证 Legacy 与依赖 hash。
- 只记录生产链接依赖：会遗漏 build tool、数据、模型、媒体、archive 与生成 Artifact 的传递义务。
- 依赖托管平台可见性：平台访问条款不能替代仓库级许可证和逐项 provenance。

## Verification

- `LICENSE-STATUS.md` 明确非授权性质、内部处理边界与外部导出门禁；
- inventory 覆盖 repository、blueprint、research evidence、Legacy archive、Eigen、w64devkit、宿主验证工具链与固定 CI action；
- validator 复核 Legacy hash、字节数、510 个 ZIP entries、391 个 file entries 和零 license signal；
- validator 检查 required fields、唯一 identity、来源、状态 vocabulary、SPDX 结论与外部分发一致性；
- inventory mutation suite 以预期诊断拒绝 26 类反例，覆盖 identity/order/vocabulary、逐项完整语义投影、本地 tracked locator、`NOASSERTION` 外部分发、Legacy hash、许可证结论和 lineage；
- scientific-context suite 以预期诊断拒绝 173 类 schema、严格 JSON、contract projection、manifest/fact binding、来源完整性、适用域、独立性、lineage、authority、状态原子性、门禁钩子、外部分发、runtime consumer 与 Git index/object 反例；
- CTest、repository verification 和审查报告共同构成 gate evidence。

## Supersession rule

Product Owner 与 Architecture Lead 可以共同接受本 ADR 所定义的 fail-closed 治理，同时继续保持仓库许可证未选择。未来选择 repository license、建立贡献者协议、允许第三方分发、分享 Legacy、公开数据集或发布二进制时，必须形成单独或取代性 ADR，并同步更新 inventory、policy、门禁与迁移/通知计划。
