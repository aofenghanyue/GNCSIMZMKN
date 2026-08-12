# R0 持续执行状态

- 记录日期：2026-08-12
- 当前 gate：`R0`
- 远端集成基线：`origin/main@291cb28b064642f3e7aa14303ee30b03c8d047f0`
- 可信锚点核验：`origin/main` 精确等于锚点，已通过 ancestor 检查
- 当前任务：`R0-GOV-001`
- 当前分支：`codex/r0-gov-001`
- 当前工作树基线：`291cb28b064642f3e7aa14303ee30b03c8d047f0`
- 执行团队 shared thread：`019ff3be-4a80-7210-a14e-dac71ac15f9f`

## 已登记机器智能体

| Actor | 实际 task path | 责任范围 | 主要复核 actor |
| --- | --- | --- | --- |
| `r0-po-agent` | `/root` | Product Owner | `r0-validation-agent` |
| `r0-architecture-agent` | `/root/r0_architecture_agent` | Architecture Lead、Compiler Lead | `r0-validation-agent` |
| `r0-science-agent` | `/root/r0_science_agent` | Scientific Authority、Model SDK Lead | `r0-architecture-agent` |
| `r0-validation-agent` | `/root/r0_validation_agent` | Validation Lead、Runtime/Numerics Lead、Evidence/Workflow Lead | `r0-po-agent` |

授权权威记录为 `docs/governance/r0-owner-authorization.json`。四个 actor 均明确标记为机器智能体；同一事项的实现与最终复核分离；Scientific Authority 与 Architecture Lead 使用独立 actor 和 task binding。

## 已完成任务

- Bootstrap 四项任务保持 `done`。
- R0 当前仍有 0 项 `done`；六项技术基线处于 `review`，七项任务处于 `planned`。

## 当前纵向切片

`R0-GOV-001` 正在完成仓库所有者授权登记、角色映射、ADR-0009 修订和 repository guard。ADR 在独立复核与 disposition 落库前保持 `Proposed`。工具链 profile 在 commit-bound Hosted CI 证据闭合前保持 `candidate-not-supported`。

## 验收与测试结果

在干净的可信基线上重新配置后：

- `cmake --preset dev`：通过；
- `cmake --build --preset dev`：通过；
- `ctest --preset dev --output-on-failure`：9/9 通过；
- `tools/verify-repository.ps1`：通过，验证 57 个 JSON、65 个任务条目、100 个 Markdown，以及全部已登记 R0 bundle；
- 新治理 guard 的工作树验证：角色缺失槽位 0，四个授权 actor 使用四个唯一 task binding，18/18 mutation 被拒绝；ADR 与 Hosted CI 仍作为显式 blocker。

旧构建目录含 2026-08-11 的陈旧 `LastTestsFailed.log`，其中测试名称已过期。本轮结果以重新配置后的 9 项 CTest 和当前 `LastTest.log` 为准。

## 已合并 PR

- PR #1：已合并到 `main@291cb28b064642f3e7aa14303ee30b03c8d047f0`。

## 下一项满足依赖的工作

1. 由 `r0-validation-agent` 独立复核 `R0-GOV-001` 治理 diff 和 mutation；
2. 由授权 Product/Architecture actor 记录 ADR-0009 disposition；
3. 推送 `codex/r0-gov-001`，取得固定 Windows/Linux profile 的 commit-bound CI 结果；
4. 闭合任务 acceptance/evidence 后将 `R0-GOV-001` 依次推进到 `done` 并合并；
5. 从最新 `main` 开始下一项阶段 A 收口切片。

## 真实 blocker

- `ADR-0009` 尚待独立复核和正式 disposition；
- 当前工作树 commit 尚无固定 runner CI；
- ADR-0004～0008 与 `RECON-DEC-001`～`009` 尚未全部裁决；
- repository、blueprint、Legacy 和科学来源的许可结论仍为 `NOASSERTION`，外部分发继续阻断；
- minimal 3DoF、Legacy behavior、YYZ 与 CAVH bundle 尚未全部达到 executable；
- G0/G1 尚未评审。

## R0 完成条件剩余项

- 13 个 `R0-*` 任务全部闭合；
- ADR-0004～0009 和九项 reconciliation decision 全部形成有效决定；
- 架构、Legacy、科学、性能、确定性和 provenance 证据全部闭合；
- Windows/Linux CI、mutation、clean-checkout 和完整 repository verification 通过；
- G0、G1 均形成 `Passed` 记录；
- gate record、backlog 与 `project-manifest` 原子一致；
- `main` 与远端同步且工作区干净；
- R1 仅按正式 gate record 解锁，生产实现保持未启动。

## 恢复规则

会话恢复或上下文压缩后，依次读取本文件、`git status`、近期提交和 `docs/tasks/backlog.json`，再从“下一项满足依赖的工作”继续。
