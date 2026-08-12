# R0 权威解锁包

- 状态：`Superseded as current authority input / retained as historical evidence`
- 准备日期：2026-08-10
- 技术基线 commit：`a05b65f36924f9a0f9e68167e65a0960b0d4f5b4`
- 工作分支：`codex/r0-first-wave-reconciliation`
- 当前 gate：`R0`

## 当前处置

本包记录 2026-08-10 基线上的历史阻塞状态。2026-08-12，仓库所有者已经通过可校验指令授权四个独立机器智能体承担 R0 责任角色；当前权威记录为 [`r0-owner-authorization.json`](../governance/r0-owner-authorization.json)、[`role-assignments.json`](../team/role-assignments.json) 与 [`r0-execution-state.md`](r0-execution-state.md)。本包中 human-only、禁止 AI 决策、无 push 授权、旧分支和旧 commit 等陈述不再表达当前治理状态。

本包继续保留当时的证据缺口清单。rights/provenance、外部分发、Hosted CI retention、任务 acceptance 和阶段门仍需各自闭合；机器智能体授权不会改变这些 fail-closed 边界。

## 历史用途与边界

本包把当时 R0 已通过机器验证的技术成果和待授权决定整理到一个审查入口。仓库所有者随后提供的 2026-08-12 授权已经取代本包中的责任主体资格假设。

本文件不充当当前角色登记、ADR decision、task approval、waiver、gate decision、许可证或外部分享许可。有效决定需要由已授权的对应 actor 对冻结 commit 明确作出，落实到权威文件，并接受不同 actor 的独立复核。

在上述技术基线上：

- 8 个 required roles 的 16 个 assignee/reviewer 槽位仍为空；
- `R0-GOV-001`、`R0-GOV-002`、`R0-ARCH-001`、`R0-LEG-001`、`R0-SCI-001`、`R0-SPEC-001` 均为 `review`；
- `R0-GATE-001` 的 9 个直接依赖为 3 个 `review`、6 个 `planned`、0 个 `done`；
- ADR-0004～ADR-0009 均为 `Proposed`；
- 当前 commit 的 hosted CI 尚无 retained run evidence；
- repository license 未选择，全部 8 个 provenance inventory item 的结论仍为 `NOASSERTION`；
- G0/G1 尚未具备正式评审条件，R1 保持锁定。

权威状态分别来自 [role assignments](../team/role-assignments.json)、[R0 backlog](../tasks/backlog.json)、[ADR index](../adr/README.md)、[provenance inventory](../governance/provenance-inventory.json)、[open decisions](open-decisions.md) 和 [project manifest](../../project-manifest.json)。如果本包与这些来源不一致，以这些来源及其受审查 commit 为准。

## 1. 历史角色指派输入

本节表格保存旧基线上的空槽位。当前角色映射以授权登记和角色登记为准。已登记机器 actor 可以承担责任角色；未登记别名、职位占位符和同一 actor/task 自审继续无效。Scientific Authority 与 Architecture Lead 保持独立。

下表仅作历史回复工作表，不再作为待填写入口。

| Role id | Required | Assignee | Reviewer | 可解析身份或决定证据 |
| --- | --- | --- | --- | --- |
| `product_owner` | yes |  |  |  |
| `scientific_authority` | yes |  |  |  |
| `architecture_lead` | yes |  |  |  |
| `model_sdk_lead` | yes |  |  |  |
| `compiler_lead` | yes |  |  |  |
| `runtime_numerics_lead` | yes |  |  |  |
| `evidence_workflow_lead` | yes |  |  |  |
| `validation_lead` | yes |  |  |  |
| `application_lead` | no |  |  |  |

每项至少需要：role id、assignee、reviewer、决定人、决定日期和适用范围。姓名字符串应能通过团队认可的目录、账号或签署记录解析到人员；不应把个人敏感信息直接写进公开仓库。

## 2. ADR disposition 输入

每份 ADR 只能选择 `accept-as-written`、`revise` 或 `reject`。`revise` 必须列出精确修改；`reject` 必须说明替代方案、受影响任务和迁移/回退。没有完整角色指派和对应 owner 决定时，ADR 保持 `Proposed`。

| ADR | 冻结内容 | Required decision owners | 主要影响 | Disposition / notes |
| --- | --- | --- | --- | --- |
| [ADR-0004](../adr/0004-r0-json-schema-contracts.md) | R0 JSON Schema、fixture/oracle/proof contract 与依赖无关验证 | Architecture Lead、Validation Lead | `R0-SPEC-001`，`RECON-DEC-001`～`003` |  |
| [ADR-0005](../adr/0005-derived-architecture-baseline.md) | glossary/ADR/CMake 派生的确定性架构基线 | Architecture Lead | `R0-ARCH-001/002`，`RECON-DEC-006/007` |  |
| [ADR-0006](../adr/0006-si-frame-and-simulation-time-conventions.md) | SI、frame、方向、simulation time 与 step policy | Scientific Authority、Architecture Lead | `R0-SCI-*`，`RECON-DEC-009` |  |
| [ADR-0007](../adr/0007-passive-hamilton-quaternion-convention.md) | passive Hamilton quaternion convention | Scientific Authority | `R0-SCI-001`，`RECON-DEC-004` |  |
| [ADR-0008](../adr/0008-internal-default-license-and-provenance-gate.md) | 未知权利 fail closed、lineage 与 external export gate | Product Owner、Architecture Lead | `R0-GOV-002`、Legacy/source rights、D-002/D-012 |  |
| [ADR-0009](../adr/0009-accountable-roles-and-candidate-toolchain.md) | accountable roles 与候选 Windows/Linux toolchain | Product Owner、Architecture Lead | `R0-GOV-001`、hosted CI、D-003 |  |

每个 disposition 至少记录：ADR id、决定、决定人及角色、reviewed commit、日期、理由、修改或替代引用。接受 ADR 并不自动把关联 task 标为 `done`；task 的 acceptance、evidence 和 reviewer 仍需逐项闭合。

## 3. 重建差异决定

[first-wave reconciliation audit](../quality/r0-first-wave-reconciliation-audit.md) 已安全移植可复用的反例和边界检查，但没有替 owner 选择公共语义。以下九项均保持 open。若选择改变当前 contract，必须先确定 ADR、schema/version、迁移、兼容、fixture/oracle 和 failure-test 影响。

| Decision id | 当前 fail-closed 基线 | Owner 必须明确的决定 | Required owners |
| --- | --- | --- | --- |
| `RECON-DEC-001` | 三份 current v1 schema 的 `$id` 和 field graph 不变 | 保留 current v1，或以新版本采用另一 graph；同时给出双版本验证和迁移/退役策略 | Architecture Lead、Validation Lead |
| `RECON-DEC-002` | executable/qualified evidence 目前只要求引用可解析到仓库文件；没有公共多 scheme grammar | 允许的 repository/local/external scheme、base、规范化、安全边界、离线行为和版本策略 | Architecture Lead、Evidence/Workflow Lead |
| `RECON-DEC-003` | PlanProof premises 保持 current object-map contract | 是否引入 typed prerequisite graph/index；若引入，确定 cycle、identity、版本和 consumer migration | Architecture Lead、Compiler Lead |
| `RECON-DEC-004` | 正 z coefficient 的 quarter-turn fixture 映射 `x -> -y`；candidate 的正角 adapter 使用负 z coefficient | 选择公共 axis-angle 正号和 adapter 名；也可保留两个明确命名且不可混用的 adapter | Scientific Authority |
| `RECON-DEC-005` | aggregate provenance inventory 不声明逐 artifact scientific context 或 independent-reference | context/independence 放入 manifest、独立 registry 或其他 contract；确定必填字段、依据和批准边界 | Scientific Authority、Product Owner |
| `RECON-DEC-006` | `packages_user`、`composition_root` 只是 logical rule labels，不是 ADR-0003 physical modules | 其 representation、source authority 和与物理 DAG 的关系；若晋升 physical module，必须修订 ADR-0003 | Architecture Lead |
| `RECON-DEC-007` | 保留 current 22 个 Legacy ownership mappings；candidate 额外映射不进入 authority | 对 3 个 owner split、2 个 logical routes 和 6 项 absent-name responsibilities 逐项保留/迁移/拒绝，并给出 glossary 与 owner 变更 | Architecture Lead、Validation Lead |
| `RECON-DEC-008` | workflow 使用固定 runner 和 pinned checkout，但当前 commit 未运行，也未定义证据包 retention | 批准或修改 runner/action profile；决定 run log 是否足够，或增加 pinned artifact action、证据内容、保留期和内部 hash receipt | Product Owner、Validation Lead |
| `RECON-DEC-009` | fixture 测试 half-open validity，但只标记为 Proposed | half-open 是否成为 public time contract；明确端点、空区间、跨 clock、采样和迁移语义 | Scientific Authority、Architecture Lead |

每项回复至少包含：decision id、`keep-current | adopt-versioned-change | reject-alternative`、决定人、reviewed commit、理由、所需 ADR/task 和迁移引用。不能用“candidate 测试更多”或“本地测试通过”替代语义决定。

## 4. 来源、权利与 G1 决策输入

在 ADR-0008 被有权 owner 接受并且下列权利链闭合前，内部现状可继续 fail closed，但不得对外发布、复制或分发代码、蓝图、Legacy archive、fixture、报告或二进制。现有 GitHub 可访问性不是许可证授权。

| Scope | 当前结论 | 需要提供的权威输入 |
| --- | --- | --- |
| repository governed content | `NONE` / `NOASSERTION`，外部分发阻断 | copyright/贡献权属、可许可范围、目标受众与 D-002 repository license 选择 |
| architecture blueprint | 来源已定位，许可与作者授权未结论 | 作者/权利人、导入与修改依据、允许的内部/外部分发范围、证据引用 |
| frozen Legacy archive | evidence-only，禁止再分发 | source owner、合法访问/处理依据、D-012 分享决定；无授权时确认永久保持内部 evidence-only |
| minimal 3DoF、YYZ、CAVH scientific sources | executable independent source/asset 尚未提供 | 精确版本/版次、公式或数据位置、单位/frame/time 适用域、使用许可、独立性依据和可保存 hash |
| generated R0 research evidence | 继承全部 upstream restriction | 逐 artifact scientific context、lineage、目标 consumer、保留期与 export disposition |
| Eigen、w64devkit、host tools、CI action | 外部执行或 isolated reproduction，项目不再分发 | 若继续不再分发则确认边界；若产品采用/打包则提供逐组件 license/NOTICE/source-offer 审查 |

同一次 owner review 可以关闭 [open decisions](open-decisions.md) 中到 G1 截止的 `D-001`（产品/namespace/可执行文件名）、`D-002`（许可证）、`D-003`（支持 profile）、`D-004`（依赖锁）和 `D-012`（Legacy 分享），但每项仍需各自的决定、owner、日期和落库位置。来源材料只提交引用或受控存放位置，不应把秘密、个人信息或无分发权的原始材料直接加入仓库。

## 5. Remote push 与 hosted CI 授权

本地 commit 不等于远端发布。推送、开 PR、合并、打 tag 和改变仓库设置是彼此独立的动作。当前没有 push 授权；默认保持本地分支，不 force-push、不合并、不打 tag。

远端执行输入应明确填写：

| Field | 当前值 / 可选决定 |
| --- | --- |
| Push authorization | `yes | no` |
| Remote | 默认 `origin` → `https://github.com/aofenghanyue/GNCSIMZMKN.git` |
| Branch | 默认 `codex/r0-first-wave-reconciliation` |
| Allowed operation | 默认仅普通 push；PR、merge、tag、force-push 均需另行授权 |
| Visibility/audience | 由 Product Owner 确认，且必须符合 ADR-0008/rights 约束 |
| Hosted profiles | 候选 `ubuntu-24.04` + GCC 13、`windows-2025-vs2026` + MSVC 19.5x |
| Workflow identity | [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml)，checkout v6.0.2 pinned commit `de0fac2e4500dabe0009e67214ff5f5447ce83dd` |
| Retained evidence | run URL/id、exact commit、job conclusion、runner image、tool/compiler identity、commands、test/repository-check logs、必要 artifact hashes |
| Retention | 由 Product Owner + Validation Lead 决定天数、保存位置和访问范围 |

技术建议是在批准的固定 runner 上运行当前精确 commit，并把 run identity 与下载后 SHA-256 记录保存在内部 evidence index。若增加 artifact upload action，应先固定精确 action commit、登记 provenance，并审查 retention；不能把 candidate branch 的既有成功 run 转移到当前 commit。

## 6. 合法解锁顺序

1. 仓库所有者提供真实角色指派；更新 role registry 并通过 `tools/validate-team-toolchain.ps1`。
2. 对 ADR-0004～ADR-0009 和 `RECON-DEC-001`～`009` 作出具名决定；对任何 revision 先实现、验证和重新冻结 commit。
3. 闭合 repository/blueprint/Legacy/scientific source rights；ADR-0008 未接受且 D-002/D-012 未闭合时保持 internal-only。
4. 由有权 reviewer 针对冻结 commit 审查 6 个 `review` tasks。每项记录 acceptance、success/failure/boundary tests、evidence/hash、reviewer、结论和日期；只有完整通过的 task 才能在受审查 commit 中变为 `done`。
5. 依赖闭合且 owner 已指定 assignee/target review date 后，按 backlog 原子激活 planned tasks：`ARCH-002` 依赖 `ARCH-001 + SPEC-001`；`LEG-002` 依赖 `LEG-001 + SPEC-001`；`SCI-002` 依赖 `SCI-001 + SPEC-001`；`SCI-004` 依赖 `SCI-001 + LEG-001 + SPEC-001`；`SCI-003` 还需等待 `LEG-002`；`PERF-001` 还需等待 `GOV-001 + SCI-002`。
6. 获得明确 push 授权后推送精确分支；在批准的 hosted profiles 上运行并保留与 commit 绑定的证据。
7. 仅在 9 个直接依赖全部 `done`、证据索引闭合且角色/ADR/rights/hosted evidence 均有效后召集 G0；G0 `Passed` 后再进行 G1。
8. 只有具名 G1 decision 为 `Passed` 时，才能在同一授权变更中更新 backlog、`project-manifest.current_gate` 和精确 R1 unlock set。

正式 task review 最少记录：task id、`approved | changes-requested`、reviewed commit、reviewer identity/role、逐项 acceptance 结论、evidence refs/hash、未闭合问题、日期。正式 gate decision 使用 [gate decision template](../quality/gate-decision-template.md)，并遵守 [R0 G0/G1 readiness audit](../quality/r0-g0-g1-readiness-audit.md)。

## 7. 冻结 commit 的复核命令

在干净 checkout 和声明的 profile 中执行：

```powershell
git status --short
& .\tools\bootstrap.ps1 -Preset dev
& .\tools\bootstrap.ps1 -Preset release
& .\tools\verify-repository.ps1
git diff --check
```

`verify-repository.ps1` 已包含 schema、architecture、Legacy、scientific、provenance 和 team/toolchain validators。正式审查还需保存 OS、compiler、CMake、generator、PowerShell/Python、命令、commit、binary/report/artifact hash 与原始日志；命令通过不等于 owner approval。

## 8. 最小回复清单

仓库所有者可按以下顺序一次性回复，未知项明确写 `defer`，不要猜测：

1. 9 个 role 的 assignee/reviewer（`application_lead` 可留空）及身份解析依据；
2. ADR-0004～ADR-0009 的 disposition、决定人和修改意见；
3. `RECON-DEC-001`～`009` 的选择、理由和迁移要求；
4. D-001/D-002/D-003/D-004/D-012 的决定；
5. repository、blueprint、Legacy、3DoF、YYZ、CAVH 的权利/来源证据引用；
6. 是否允许向 `origin/codex/r0-first-wave-reconciliation` 普通 push；是否另行允许 PR；
7. hosted evidence 内容、保留期和访问范围；
8. 哪些具名 reviewer 将审查 6 个 current `review` tasks，以及目标 review date。

缺少的项目继续保持当前 fail-closed 状态；部分决定不会被扩张解释为其他动作的授权。
