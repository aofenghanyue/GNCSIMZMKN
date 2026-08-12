# R0 持续执行状态

- 记录日期：2026-08-12
- 当前 gate：`R0`
- 远端集成基线：`origin/main@e0bba2b99e96a2a6ded646302b1d1424d323c362`
- 可信锚点核验：`origin/main` 是已知锚点 `291cb28b064642f3e7aa14303ee30b03c8d047f0` 的后代
- 当前任务：`R0-ARCH-001`
- 当前分支：`codex/r0-arch-001`
- 当前分支基线：`e0bba2b99e96a2a6ded646302b1d1424d323c362`
- 当前工作树：ADR-0005 与 `RECON-DEC-006/007` 技术候选、机器 review contract、严格解析与 35 项新增 mutation 已写入；ADR/decision 保持 `Proposed`，任务保持 `review`
- 当前实现 actor：`r0-po-agent`（task `/root`）；Architecture decision owner 仍为独立 `r0-architecture-agent`
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
- R0 当前有 2 项 `done`；四项技术基线处于 `review`，七项任务处于 `planned`。

## 当前纵向切片

`R0-SPEC-001` 已通过 PR #3 合并到 `main@e0bba2b99e96a2a6ded646302b1d1424d323c362`，合并后 push run `31567805325` 的 Ubuntu/Windows profile 均成功。

当前 `R0-ARCH-001` 候选保留 276 个 canonical terms、20 个 aliases、10 个 capabilities、27 个 shared symbols、22 条 Legacy ownership、ADR-0003/1 的 9 个物理模块与 22 条 CMake edges。`packages_user` / `composition_root` 只作逻辑规则标签；33 项 candidate responsibility 固定为 `22 + 3 + 2 + 6` 分类。review contract 锁定 11 个 authority/generator/derived entries、2 个 logical boundaries、零 runtime consumer 与 35 项新增 mutation。Architecture owner 与独立 Validation reviewer 仍需对冻结 commit/fileset 给出正式 disposition。

## 验收与测试结果

在干净的可信基线上重新配置后：

- `cmake --preset dev`：通过；
- `cmake --build --preset dev`：通过；
- `ctest --preset dev --output-on-failure`：9/9 通过；
- `tools/verify-repository.ps1`：当前 ARCH 技术候选工作树验证 66 个仓库 JSON、65 个任务条目、100 个 Markdown，以及全部已登记 R0 bundle；`.gitignore` 登记的生成输出目录不参与仓库文件计数；
- `tools/validate-architecture-baseline.ps1`：276 terms、20 aliases、10 capabilities、27 shared symbols、22 Legacy mappings、9 modules、22 CMake edges、15 architecture mutations、2 logical boundaries 与 35 review-contract mutations 全部通过；
- `tools/validate-r0-specs.ps1`：3 schemas、5 actual manifests、6 valid、16 invalid、9 strict failures、25 stable identities、5 identity mutations、20 contract mutations 和 25 task-acceptance mutations 全部纳入门禁；
- 新治理 guard 的工作树验证：角色缺失槽位 0，四个授权 actor 使用四个唯一 task binding，49/49 mutation 被拒绝；readiness 为 `ready`，治理 blocker 为 0；
- Hosted CI push run `31559701566`：Ubuntu 24.04/GCC 13 与 Windows 2025/VS 2026 均通过，精确 checkout 为 `32f6ade7f2feec6fb1792121773d437f6f035581`。
- Hosted CI PR run `31559704268`：两个 profile 均通过，精确 checkout 为临时 merge commit `4ecb1cafc8fab2dc385d1fb5493aa823b48b08bd`。
- receipt 永久保存 run/job URL、必需步骤、runner/image/tool/compiler identity、日志归档 hash 和 90 天上游保留设置；两次 run 均无二进制 artifact，适用范围仅为治理与工具链资格。
- 最终验收对象 `416725156d4cc14410f33d2e0fd34cc4e2d031f4` 的 push run `31562029553` 与 PR run `31562031272` 均在 Ubuntu/Windows 通过；Validation Lead 已给出 `accepted`。
- PR #2 最终 head `f3b25a6836b7e81d1dd67225bf8be2d4ca9d03b0` 的 push run `31563122772` 与 PR run `31563124749` 均在 Ubuntu/Windows 通过。
- R0-SPEC-001 技术提交 `ee7157359e689114d0259a1ae7884a315b029bc1` 的 push run `31565404481` 与 PR run `31565406888` 均在 Ubuntu/Windows 通过；PR 实际 checkout 为 merge commit `52a6630394a2c9720971bae677eea9cb2b71a674`，tree 与双 parent 已离线固化。
- R0-SPEC-001 最终提交 `d3a073f663fef212f9390c465179be2d9a005ae2` 的 push run `31567498688` 与 PR run `31567501708` 均在 Ubuntu/Windows 通过；Validation Lead 对 23 路径 Git-blob 文件集给出 `approved / merge-ready`。
- PR #3 合并提交 `e0bba2b99e96a2a6ded646302b1d1424d323c362` 的 main push run `31567805325` 在 Ubuntu 24.04/GCC 13 与 Windows 2025/MSVC 19.5x 全部通过。

旧构建目录含 2026-08-11 的陈旧 `LastTestsFailed.log`，其中测试名称已过期。本轮结果以重新配置后的 9 项 CTest 和当前 `LastTest.log` 为准。

## 已合并 PR

- PR #1：已合并到 `main@291cb28b064642f3e7aa14303ee30b03c8d047f0`。
- PR #2：已合并到 `main@611a48a23ea02ecd0c210a2b101f5c5cbf5df0e6`；`R0-GOV-001` 已完成。
- PR #3：已合并到 `main@e0bba2b99e96a2a6ded646302b1d1424d323c362`；`R0-SPEC-001` 已完成。

## 下一项满足依赖的工作

1. 完成 R0-ARCH-001 technical candidate 的 targeted/full verification，冻结 commit/fileset 并取得 Windows/Linux push/PR CI；
2. 由 `r0-architecture-agent` 给出 ADR-0005/DEC006/007 owner disposition，由 `r0-validation-agent` 完成独立只读复核；
3. 原子形成 ADR/RECON/task acceptance，把 `R0-ARCH-001` 置为 `done`，随后合并并复核 main；
4. 继续收口 ADR-0006～0008、R0-SCI-001、R0-LEG-001 与 R0-GOV-002。

## 真实 blocker

- ADR-0005～0008 与 `RECON-DEC-004`～`007`、`009` 尚未全部裁决；ADR-0005/DEC006/007 当前仅有技术候选；
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
