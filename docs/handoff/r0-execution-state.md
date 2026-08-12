# R0 持续执行状态

- 记录日期：2026-08-12
- 当前 gate：`R0`
- 远端集成基线：`origin/main@611a48a23ea02ecd0c210a2b101f5c5cbf5df0e6`
- 可信锚点核验：`origin/main` 是已知锚点 `291cb28b064642f3e7aa14303ee30b03c8d047f0` 的后代
- 当前任务：`R0-SPEC-001`
- 当前分支：`codex/r0-spec-001`
- 当前分支基线：`611a48a23ea02ecd0c210a2b101f5c5cbf5df0e6`
- 当前工作树：v1 contract lock、repository-root evidence locator 与 20 项新增 mutation 已实现；ADR-0004 和任务继续保持待独立复核状态
- ADR-0009 冻结审查 commit：`d4f1a6b105b680a7e7f32925d90a44c0f85f57e0`
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
- R0 当前有 1 项 `done`；五项技术基线处于 `review`，七项任务处于 `planned`。

## 当前纵向切片

`R0-SPEC-001` 正在收口。Architecture Lead 已完成初始技术实现：三份 Fixture v1 schema 原始字节保持不变，machine lock 固定 identity、instance version、raw hash 和 field graph；executable evidence 统一解析为仓库根相对、Git 跟踪、非空普通 blob；PlanProof v1 `premises` 保持 scalar object-map snapshot；产品/runtime consumer 维持为零。ADR-0004 与 `RECON-DEC-001`～`003` 尚未接受，backlog 保持 `review`，YYZ manifest 继续登记该 open task。

## 验收与测试结果

在干净的可信基线上重新配置后：

- `cmake --preset dev`：通过；
- `cmake --build --preset dev`：通过；
- `ctest --preset dev --output-on-failure`：9/9 通过；
- `tools/verify-repository.ps1`：当前 SPEC 工作树通过，验证 60 个仓库 JSON、65 个任务条目、100 个 Markdown，以及全部已登记 R0 bundle；`.gitignore` 登记的生成输出目录不参与仓库文件计数；
- `tools/validate-r0-specs.ps1`：通过，3 schemas、5 actual manifests、6 valid、16 invalid、9 strict failures、25 stable identities、5 identity mutations 和 20 contract mutations 全部符合预期；
- 新治理 guard 的工作树验证：角色缺失槽位 0，四个授权 actor 使用四个唯一 task binding，49/49 mutation 被拒绝；readiness 为 `ready`，治理 blocker 为 0；
- Hosted CI push run `31559701566`：Ubuntu 24.04/GCC 13 与 Windows 2025/VS 2026 均通过，精确 checkout 为 `32f6ade7f2feec6fb1792121773d437f6f035581`。
- Hosted CI PR run `31559704268`：两个 profile 均通过，精确 checkout 为临时 merge commit `4ecb1cafc8fab2dc385d1fb5493aa823b48b08bd`。
- receipt 永久保存 run/job URL、必需步骤、runner/image/tool/compiler identity、日志归档 hash 和 90 天上游保留设置；两次 run 均无二进制 artifact，适用范围仅为治理与工具链资格。
- 最终验收对象 `416725156d4cc14410f33d2e0fd34cc4e2d031f4` 的 push run `31562029553` 与 PR run `31562031272` 均在 Ubuntu/Windows 通过；Validation Lead 已给出 `accepted`。
- PR #2 最终 head `f3b25a6836b7e81d1dd67225bf8be2d4ca9d03b0` 的 push run `31563122772` 与 PR run `31563124749` 均在 Ubuntu/Windows 通过。

旧构建目录含 2026-08-11 的陈旧 `LastTestsFailed.log`，其中测试名称已过期。本轮结果以重新配置后的 9 项 CTest 和当前 `LastTest.log` 为准。

## 已合并 PR

- PR #1：已合并到 `main@291cb28b064642f3e7aa14303ee30b03c8d047f0`。
- PR #2：已合并到 `main@611a48a23ea02ecd0c210a2b101f5c5cbf5df0e6`；`R0-GOV-001` 已完成。

## 下一项满足依赖的工作

1. 提交 `R0-SPEC-001` 初始技术冻结，取得精确 commit 的 Windows/Linux CI；
2. 由 `r0-architecture-agent` 与 `r0-validation-agent` 对同一 commit/fileset 形成 ADR-0004、`RECON-DEC-001`～`003` disposition；
3. 原子写入 Accepted、task acceptance、`done` 状态，并从 YYZ `open_tasks` 移除 `R0-SPEC-001`；
4. 合并 SPEC PR，随后并行收口 ADR-0005～0008 与其余四个 review 工作包。

## 真实 blocker

- `R0-SPEC-001` 初始技术冻结尚未形成 commit-bound 独立 disposition 与当前 head Hosted CI；
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
