# ADR-0008: Public GitHub collaboration with platform-scoped rights

- Status: Accepted
- Date: 2026-08-10
- Revised: 2026-08-15
- Decision date: 2026-08-15
- Owner: Repository owner
- Related task: R0-GOV-002
- Architecture references: 03 §4、05 §3、08 §11–§12、08 §21

## Context

仓库根目录没有分发许可证。GitHub 的许可证说明指出，无许可证仓库仍受默认版权规则约束；GitHub 服务条款同时赋予 public repository 用户通过平台功能查看、使用、显示、运行和 fork 内容的权利。仓库级通用许可证与 GitHub 平台范围内的访问和协作权利需要分开记录。[GitHub licensing a repository](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository) [GitHub Terms of Service §D](https://docs.github.com/en/site-policy/github-terms/github-terms-of-service#d-user-generated-content)

2026-08-15 的只读核对确认 origin `https://github.com/aofenghanyue/GNCSIMZMKN.git` 当前公开可见，并已有公开 fork `https://github.com/zbyandmoon/GNCSIMZMKN`。当前身份对 origin 具有管理权限，对该 fork 只有读取权限。仓库所有者明确要求 origin 保持 public，用于与 `zbyandmoon` 的 GitHub 协同。该选择取代 private transition 路线。

public origin 已经形成持续的互联网访问入口。仓库所有者当前选择接受这项公开暴露，并把允许范围收敛到 GitHub 平台提供的协作功能。仓库仍没有通用分发许可证，后续 release、二进制包、数据包或平台外源码包需要单独的权利和许可决定。

当前 Git 跟踪内容可完整划分为三类：仓库源码/文档/fixture/oracle、导入的架构蓝图、`reference/legacy/`。冻结 Legacy ZIP 的来源身份和 SHA-256 已固定，实时扫描仍未发现许可证命名文件或强许可证文本信号；该扫描只表达检测事实，不提供使用或再分发许可。

R1 Foundation 依据 ADR-0012 使用仓库外安装的 Eigen 3.4.0 header-only 构建依赖。R0 验证使用宿主 Python、PowerShell、CMake 和编译器；固定 CI 使用 `actions/checkout`；隔离 Legacy 复现另用 Eigen 3.4.0 与 w64devkit 2.9.1。依赖源码与安装目录都不进入当前 Git 跟踪内容。Eigen 3.4.0 上游说明其主体采用 MPL-2.0，部分文件采用 BSD 或 LGPL；w64devkit 说明生成的 Windows binary 可能携带 runtime 许可义务，因此未来二进制分发必须按实际链接内容复核。[Eigen 3.4.0 COPYING.README](https://gitlab.com/libeigen/eigen/-/raw/3.4.0/COPYING.README) [w64devkit v2.9.1 licensing](https://github.com/skeeto/w64devkit/tree/v2.9.1#licenses)

旧版 ADR 引入双角色批准、不可变回执、报告哈希锁和大量治理 mutation。ADR-0010 已撤销这类执行方式。本修订只保留会影响实际分发边界的检查。

## Decision

仓库所有者接受 G1“public GitHub 协同、仅使用平台范围权利、当前不授予通用许可证”的范围。

1. origin 保持 public，允许通过 GitHub 页面、fork、issue、review 和 pull request 开展与 `zbyandmoon` 的协同。GitHub 服务条款定义这些平台范围权利。
2. 仓库当前不选择通用分发许可证。public visibility 不增加 GitHub 服务条款之外的许可承诺，仓库内容、架构蓝图和 Legacy reference 的许可证结论保持 `NOASSERTION`。
3. 实现智能体可以继续本地实现、测试和提交。push、merge、tag、release 以及远端设置变更仍需用户明确授权。
4. `external_distribution: blocked` 专指 public GitHub collaboration 之外、由本项目主动形成的 release、源码/二进制 bundle、数据集、报告附件或 Legacy archive 分发。该字段不否认已经存在的 public GitHub 访问和 fork。
5. Legacy 保持只读、evidence-only，不进入 release 或独立分发候选，直到权属和许可证据闭合。现有 public repository 中的 Legacy 路径属于已接受的公开暴露事实，不形成权利清结论。
6. Eigen、w64devkit、宿主工具链和 CI action 只按已记录用途在仓库外执行。Eigen 3.4.0 可以按 ADR-0012 参与 Foundation 本地与 CI 构建；任何 vendoring、runtime bundling 或二进制发布都需要新的直接复核。
7. fixture、oracle、benchmark 和报告继续记录来源。上游权利待确认时，派生内容不能进入独立 release 或平台外分发候选。
8. 通过 issue、fork 或 pull request 形成的第三方贡献可以用于当前 GitHub 协同；实际合并贡献并纳入未来许可或 release 候选时，需要确认贡献者对相应内容的权利。当前不新增 CLA、DCO 或回执机制。
9. SPDX `NONE` 表示文件内未发现许可证信息；`NOASSERTION` 表示尚未形成结论。两者都不提供通用分发许可。[SPDX File Information](https://spdx.github.io/spdx-spec/v2.2.2/file-information/)

## Consequences

- R0-GOV-002 的 GitHub 协同、通用许可和 release 边界已经闭合，可以作为 G0/G1 gate 输入。
- public origin 与已接受范围一致，不再形成 R0 gate blocker。已存在的 public fork 是当前协同路径和持续公开暴露事实。
- 当前 G1 不要求选择开源或其他通用分发许可证。未来若增加 release、安装包、平台外源码包或其他交付渠道，仓库所有者需要以真实候选范围为输入修改本决定，并逐项闭合仓库权属、Legacy、蓝图、贡献权属、第三方 notices 和实际二进制 runtime。
- GitHub 协同方对内容的可用权利受 GitHub 服务条款和适用法律约束；本 ADR 不提供法律意见。
- 本决定不推进 R1～R8，也不替代仓库所有者的阶段门决定。

## Executable evidence

`tools/validate-license-provenance.ps1` 直接读取 Git 跟踪文件、CMake、CI workflow、Legacy archive、复现环境和 `docs/governance/provenance-inventory.json`。它验证：

- 每个跟踪文件恰好进入一个实际分发范围；
- 当前唯一跟踪 binary/archive 是明确登记的 Legacy ZIP；
- `find_package(Python3)` 与固定 `actions/checkout` 都有外部输入记录；
- CMake 没有未登记的下载式依赖；
- Legacy SHA-256 与来源清单一致，并重新扫描许可证信号；
- public origin、owner disposition 与 GitHub collaboration scope 一致；
- 同一判定器拒绝把平台协同扩大为通用外发、Legacy 独立外发、未登记 binary vendoring 和下载式 CMake 依赖。

默认验证成功表示 public GitHub collaboration 边界和来源事实闭合，同时明确返回 general external distribution blocked。`-RequireExternalReady` 仍以退出码 2 拒绝 release 或平台外分发。`tools/validate-r0-gate-readiness.ps1 -RequireDecisionReady` 在技术输入通过后不再受到远端可见性阻断。

## Supersession

只有仓库所有者改变 public GitHub collaboration、通用许可或 release 范围时才修订或替代本 ADR。任何新分发路线都需要以真实候选文件和交付形式为输入完成直接权利复核。
