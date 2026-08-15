# ADR-0008: Internal-development G1 distribution boundary

- Status: Accepted
- Date: 2026-08-10
- Revised: 2026-08-15
- Decision date: 2026-08-15
- Owner: Repository owner
- Related task: R0-GOV-002
- Architecture references: 03 §4、05 §3、08 §11–§12、08 §21

## Context

仓库根目录没有分发许可证。GitHub 的许可证说明指出，无许可证仓库仍受默认版权规则约束；公开仓库用户同时具有 GitHub 服务条款提供的查看和 fork 权限。仓库级许可证与平台访问权限需要分开判断。[GitHub licensing a repository](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository)

2026-08-15 的只读核对确认 origin `https://github.com/aofenghanyue/GNCSIMZMKN.git` 当前公开可见。现有公开暴露已经发生，后续本地策略无法撤回既有副本。公开 origin 与内部开发范围存在冲突，远端可见性变更仍需仓库所有者单独授权。

当前 Git 跟踪内容可完整划分为三类：仓库源码/文档/fixture/oracle、导入的架构蓝图、`reference/legacy/`。冻结 Legacy ZIP 的来源身份和 SHA-256 已固定，实时扫描仍未发现许可证命名文件或强许可证文本信号；该扫描只表达检测事实，不提供使用或再分发许可。

当前 greenfield C++ target 没有产品第三方库。R0 验证使用宿主 Python、PowerShell、CMake 和编译器；固定 CI 使用 `actions/checkout`；隔离 Legacy 复现另用 Eigen 3.4.0 与 w64devkit 2.9.1。它们都在仓库外执行，没有进入当前项目分发包。Eigen 3.4.0 上游说明其主体采用 MPL-2.0，部分文件采用 BSD 或 LGPL；w64devkit 说明生成的 Windows binary 可能携带 runtime 许可义务，因此未来二进制分发必须按实际链接内容复核。[Eigen 3.4.0 COPYING.README](https://gitlab.com/libeigen/eigen/-/raw/3.4.0/COPYING.README) [w64devkit v2.9.1 licensing](https://github.com/skeeto/w64devkit/tree/v2.9.1#licenses)

旧版 ADR 引入双角色批准、不可变回执、报告哈希锁和大量治理 mutation。ADR-0010 已撤销这类执行方式。本修订只保留会影响实际分发边界的检查。

## Decision

仓库所有者接受 G1“仅内部开发、停止新增外部分发”的范围。

1. 已授权工作区内可以继续实现、测试、保存 provenance 和开展内部评审。
2. 不向公开远端新增 push，不发布 release，不发送公开源码/二进制附件、数据集、报告或 Legacy archive。
3. 仓库内容、架构蓝图和 Legacy reference 的许可证结论保持 `NOASSERTION`，外部分发状态保持 `blocked`。本决定不构成许可证授予。
4. Legacy 保持只读、evidence-only，并从任何未来外部分发候选中单独排除，直到权属和许可证据闭合。
5. Eigen、w64devkit、宿主工具链和 CI action 只按已记录用途在仓库外执行。任何 vendoring、runtime bundling 或二进制发布都需要新的直接复核。
6. fixture、oracle、benchmark 和报告继续记录来源。上游权利待确认时，派生内容继承外部分发阻断。
7. public origin 需要后续处置。实现智能体在获得远端可见性变更的明确授权前，只记录这一阻断，不改变远端状态。
8. SPDX `NONE` 表示文件内未发现许可证信息；`NOASSERTION` 表示尚未形成结论。两者都不提供分发许可。[SPDX File Information](https://spdx.github.io/spdx-spec/v2.2.2/file-information/)

## Consequences

- R0-GOV-002 的内部/外部处理规则已经闭合，可以作为 G0/G1 gate 输入。
- 当前 public origin 的可见性与已接受范围不一致，在远端处置完成前阻断内部范围的一致性检查。
- 当前 G1 不需要选择公开分发许可证。未来若扩大为公开或受限外发，仓库所有者需要以窄 ADR 修改本决定，并逐项闭合仓库权属、Legacy、蓝图、贡献权属、第三方 notices 和实际二进制 runtime。
- 本决定不推进 R1～R8，也不替代仓库所有者的阶段门决定。

## Executable evidence

`tools/validate-license-provenance.ps1` 直接读取 Git 跟踪文件、CMake、CI workflow、Legacy archive、复现环境和 `docs/governance/provenance-inventory.json`。它验证：

- 每个跟踪文件恰好进入一个实际分发范围；
- 当前唯一跟踪 binary/archive 是明确登记的 Legacy ZIP；
- `find_package(Python3)` 与固定 `actions/checkout` 都有外部输入记录；
- CMake 没有未登记的下载式依赖；
- Legacy SHA-256 与来源清单一致，并重新扫描许可证信号；
- 同一判定器拒绝内部 G1 范围绕过、Legacy 外发、未登记 binary vendoring 和下载式 CMake 依赖。

默认验证成功表示内部工作区事实闭合，同时明确返回 external distribution blocked。`-RequireExternalReady` 在已接受的内部 G1 范围下以退出码 2 拒绝外部分发。

## Supersession

只有仓库所有者明确扩大分发范围时才修订或替代本 ADR。任何外发路线都需要以真实候选文件和交付形式为输入完成直接权利复核。
