# R0-LEG-002 Legacy 行为 oracle 捕获计划

- 文档状态：Implementation tracking
- 实现成熟度：`ORACLE-YYZ-PUBLISH-01`、`ORACLE-YYZ-PHASE-02` 与 `ORACLE-YYZ-SYNC-03` 已达到 `executable`，`ORACLE-YYZ-GROUP-04`、`ORACLE-YYZ-CSV-05` 与 `ORACLE-YYZ-STOP-06` 处于 `capturing`；`ORACLE-SIMFLOW-07` 尚未实现；不得作为完整 G1 或 runtime migration pass evidence
- 任务：`R0-LEG-002`（backlog 为 `in_progress`）
- 日期：2026-08-14
- Authority owner role：Validation Lead
- 协作 owner roles：Runtime Numerics Lead、Scientific Authority、Architecture Lead

## 1. 目标与证据边界

本计划把冻结 Legacy 中值得迁移的运行时事实转成可执行、可复核的 oracle bundle，同时明确退出旧类名、节点数量、priority/registration 偶然顺序、CSV 列序、free-text reason、目录命名和 old Mission shape。oracle 保护科学、时间、事务和重放语义；旧实现拓扑不在保护范围内。

每条证据分三层：冻结来源事实、隔离 probe 的运行时观察、经 owner 批准的迁移处置。源码阅读可以解释 trace，但不能代替 trace；Legacy 运行结果可以证明旧路径，却不能伪造尚未实现的 target `RunOutcome`、`IntegrationScopePlan` 或 deterministic CaseId。

本计划不修改冻结 Legacy、不定义新的产品 runtime 契约，也不批准任何数值 tolerance 或 Preserve/Fix/Retire 决策。

## 2. 冻结来源与 entry 身份

来源 archive：

- Git head：`a63621c368aa8e7889547689bcce9c7686b886ac`；
- ZIP SHA-256：`2159a324fd897e4bd508c140a36c9165d744e4e4e61861c5b568201707f988e5`；
- ZIP prefix：`GNCZMKN-legacy-a63621c/`；
- 来源索引：[`reference/legacy/source-index.md`](../../reference/legacy/source-index.md)；
- 干净复现：[`r0-leg-001-20260810-07`](../../reference/legacy/reproduction/r0-leg-001-20260810-07/evidence-index.json)，27/27 CTest 通过。

捕获前必须从 ZIP 重新解压并核对下列 raw entry。哈希是 2026-08-10 对冻结解压 byte stream 的 SHA-256；路径相对于 archive prefix。

| Archive entry | Bytes | Raw SHA-256 | 用途 |
| --- | ---: | --- | --- |
| `framework/include/gnc/core/simulator.hpp` | 21,226 | `455b22e68fdd4511a85f9a1c20547d022261cdf3397aabfd5f85115764edae78` | step 顺序、phase、candidate/commit、publish |
| `tests/test_publish_semantics.cpp` | 27,073 | `48c5b86ea7a88a3c7702d5abdf89bcebaf8e8dfa331289317453a698f3802e08` | publish/CSV/stop/sync 种子事实 |
| `tests/test_continuous_group.cpp` | 11,404 | `6d6f01098b1b606c5e1a23eaceb501ba99f07370fc51d22fb6f5cea06b2d9d46` | shared RK candidate 和 ownership 失败 |
| `tests/test_simflow_materializer.cpp` | 6,504 | `89ea1e58e91c4a3c7c1173b3ee768c7d88cf2987ef223ca9894a0d03377bedeb` | case 物化与输入注入 |
| `tests/test_simflow_runner.cpp` | 8,091 | `9289f7710e68d0a1679a12b3adcb69ee8a8a05890e5bb1750f90c4e266354a6e` | effective mission 普通 CLI 重放 |

单个 entry hash 不能代替 archive identity；archive identity、entry identity、probe/input identity 必须同时进入 capture provenance。

canonical Legacy 捕获候选应优先复用 `R0-LEG-001` 已成功验证的 Windows x64 / w64devkit GCC 16.2.0 / CMake 4.4.2 / Ninja 1.13.2 / Eigen 3.4.0 环境，并逐项校验已记录的第三方 archive hash。任何工具链变化都要产生独立 environment identity 和 comparison finding；已有 MSVC 19.50 编译 gap 未经 Validation Lead 审查不能被当作等价 canonical 环境。未来 target 的固定平台证据与 Legacy capture provenance 分开记录，不能通过让产品 target link Legacy 来消除工具链差异。

## 3. Bundle 的概念性最小内容

以下列出验收信息需求，尚未形成已批准的新 schema：

| 信息组 | 必需内容 |
| --- | --- |
| Identity | oracle id、bundle revision、capture run id、source commit/archive/entry hashes |
| Input | 带逻辑路径的 probe、mission、asset 与 policy 清单；每项 byte hash；聚合 input hash |
| Environment | OS/arch、compiler、CMake、generator、build type、依赖身份、命令、working directory |
| Observation | raw append-only trace/dataset、exit code、stdout/stderr、开始/结束状态、双跑标识 |
| Expected | 事实 id、比较字段、事件顺序/数值/语义值、来源 ref、派生器版本 |
| Tolerance | exact/absolute/relative/invariant/convergence 类型、参数、authority owner、批准 ref |
| Disposition | 每个事实的 Preserve/Fix/Retire、五类 difference rationale、owner、approval ref |
| Mapping | legacy observable、target semantic object、target evidence 状态；未实现时显式 `pending` |
| Integrity | raw/derived/artifact index 的 SHA-256、缺失/漂移 verdict、deterministic rerun verdict |

### 3.1 当前切片的契约路由

现有 `gnczmkn.oracle-manifest/1` 继续承担集合索引，只引用实际 artifact。前五个切片使用各自 fixture-local `input.json` 与 `reference.json`，由当前 Python comparator 和 C++ probe 直接消费；每份 `reference.json` 对对应 `input.json` 原始 bytes 记录 SHA-256，并把事实级处置保存为枚举字段与独立 decision status。phase 与 continuous-group 切片另保存外部 Legacy capture harness 和两份原始 JSON trace；CSV 切片保存外部 harness、两份真实 dataset 和语义字段映射。publish 与同步提交切片的状态已接受，phase、continuous-group 与 CSV 切片保持待定。

这组 fixture-local JSON 不扩展公共 schema。后续出现第二个共享 consumer 或跨 bundle 合并需求时，再以窄 ADR 决定通用 sidecar 或 oracle schema revision。当前 `needs_owner_decision` 不能把 `R0-LEG-002` 或关联 oracle 标为完成。

## 4. 分类模型

迁移处置和差异理由是两个正交维度：

| 处置 | 允许的主要理由 | 语义 |
| --- | --- | --- |
| Preserve | `ScientificInvariant` 或已批准 `DeclaredModelChoice` | target 必须满足同一事实；实现形状可不同 |
| Fix | `ImplementationDefect` | target 明确不复现旧缺陷，必须有 defect/decision 和差异证据 |
| Retire | `AccidentalStructure` | 不进入 target expected facts，并有 guard 防止误锁定 |
| 未决 | `NeedsDecision` | 不能算 pass；阻断关联 target/gate |

一个 oracle 可同时包含 Preserve 与 Retire 事实。例如 CSV oracle 保留 `t_k` 和字段边界，却退出列序。oracle 级汇总不得掩盖事实级未决项。

## 5. 七条 oracle 捕获矩阵

| Oracle | 最小隔离输入 | 冻结种子事实 | 当前 capture | 比较/容差边界 | 当前处置 |
| --- | --- | --- | --- | --- | --- |
| `ORACLE-YYZ-PUBLISH-01` | dt=0.5 的单连续状态 + truth publisher + before/after probe | run 在每步先 publish；t0 before 见 altitude=1000/sample_time=0；t0 after 见 altitude=1004.75 但 sample_time 仍为 0 | 已捕获两个边界的 committed state/truth、事件序列和双跑输出；epoch/tick 等 target 字段保持 pending | 当前 state identity/sample time/event order exact；种子 scalar 为 1e-12 | 已接受：Preserve publish 只读与 truth 边界时间（ScientificInvariant）；Retire Legacy callback/class surface |
| `ORACLE-YYZ-PHASE-02` | 每个 phase 一个无状态 trace probe，单 tick | 宏顺序为 environment→perturbation→input→process→output→interaction→evaluation；同 phase 用 priority 后 registration order | 已用乱序注册与跨 phase priority 捕获两份真实 Legacy trace，并以独立 C++17 probe 复核；含 step/time/phase/probe/sequence | event identity/order/multiplicity/step/time exact；只从归一化中排除 rerun_index | 已接受：Preserve 宏 phase temporal convention（DeclaredModelChoice）；Retire priority 数值、registration/config tie-break 和 Legacy callback surface |
| `ORACLE-YYZ-SYNC-03` | mass'=−2、position'=mass 的两个独立连续系统，RK4，dt=1 | 最终 mass=8、position=10；源码先完成全部 pending candidate 再 setState | 已捕获 candidate-complete/commit journal、双跑输出和 early-commit 失败路径 | event partial order exact；种子 scalar 为 1e-12 | 已接受：Preserve 候选屏障与 committed-`t_k` 读取（ScientificInvariant）；Retire `pending_states`、setState 循环顺序与 Legacy 接口名称 |
| `ORACLE-YYZ-GROUP-04` | mass'=−2、position'=candidate mass 的二元连续组，RK4，dt=1 | 最终 mass=8、position=9，当前断言 1e-12；拒绝未注册成员和重复 group ownership | 已捕获两份真实四阶段 candidate/derivative 与单次 group commit trace，并用 50 位 Decimal、独立 C++17 joint RK4、valid/invalid membership cases 复核 | stage/event/membership exact；种子 scalar 1e-12 只覆盖合成用例；YYZ trajectory tolerance 由 R0-SCI-003 冻结 | 待定：建议 Preserve 共享 candidate、单次 scope commit 与唯一 membership（DeclaredModelChoice）；Retire `IContinuousGroup`、group node 和手工 vector 分发 |
| `ORACLE-YYZ-CSV-05` | constant-acceleration mission，dt=0.5/duration=1，record initial | rows: t0=(0,1000,vz=10)、t1=(0.5,1004.75,vz=9)、t2=(1,1009,vz=8)；time 1e-12、state 1e-9 | 已在固定 Legacy 环境捕获两份 byte-identical dataset；按 header 映射 fixture-local semantic field id，并以 50 位 Decimal、独立 C++17 probe 和列置换复核 | field identity/row order exact；种子 time 1e-12、state 1e-9；YYZ 字段 tolerance 由 R0-SCI-003 冻结 | 待定：建议 Preserve `t_k` 与 published-state boundary（ScientificInvariant）；Retire 列序、列名拼接、格式/目录 |
| `ORACLE-YYZ-STOP-06` | t0 即满足的 altitude stop，record initial + flush every step | termination reason 为 `stop at t0`；header + 一行，row time=0；run 中 record 在 checkTermination 前 | 已捕获两份 byte-identical t0 dataset 与两份 semantic-identical trace；终止 evaluator 在返回 true 前读回已 flush 行；独立 Python/Decimal 与 C++17 timeline 覆盖三条失败路径和 reason-text 等价路径 | event identity/order/multiplicity、step 与 row count exact；种子 scalar 1e-12；free text 不比较 | 待定：建议 Preserve 停止状态 Observation 先于对应 RunOutcome（DeclaredModelChoice）；Retire free-text reason、Legacy evaluator 与 logger/CSV surface；target artifacts 显式 pending |
| `ORACLE-SIMFLOW-07` | base mission + 两行 variation matrix + numeric perturbation materializer | `hot` case id 被物化；effective mission 含注入值；`effective_mission.json` 可由普通 `gnc_sim --config` 重放 | base/matrix/materializer/effective mission 语义 hash、命令路径、普通 replay exit/artifacts；target CaseId 映射显式 pending | canonical semantic input equality；replay result 按所选 mission oracle；CaseId 规则待 target authority | Preserve：预运行物化 + ordinary compile/run replay；Retire：`case_000001`、不生成 case manifest、old Mission shape；deterministic target CaseId 为新契约 |

### 5.1 当前没有可自动批准的 Fix

七条主事实中没有一个已由有权 owner 分类为 `ImplementationDefect`。捕获若暴露缺陷，runner 必须产生 unresolved finding，而不是自动更新 expected 或放宽 tolerance。只有带 defect id、影响范围、独立复现、owner 决策和 target evidence 的事实才能转为 Fix。

## 6. Probe 设计与非侵入性

所有 probe 在从冻结 ZIP 创建的新隔离 workspace 内编译。首选独立 harness 和 Legacy 已有测试扩展面：

- publish：以 run 前初态/上一 step after-commit hash 对照下一次 `before_step`，在 publish 已完成但离散更新尚未发生的位置观察；
- phase：外部 harness 在干净 Legacy 解压目录中注册七个无状态 `IDiscreteTask` probe，各属一个 phase，只向 trace 文件写自身事件；主构建读取冻结 trace 并运行独立 probe；
- sync：测试连续系统在 derivative/candidate 完成与 `setState` 时写 journal；probe 不读写对方 state；
- group：外部 harness 在干净 Legacy 解压目录中记录每个 RK stage 的共享 candidate/derivative、一次 group commit 和 membership failures；主构建读取冻结 trace，并运行 Decimal 与独立 C++17 reference；
- CSV：事后解析输出并映射 semantic field，不改变 logger 或列顺序；
- stop：termination evaluator、recorded dataset 与 simulator ordering 共同证明 Legacy half；
- SimFlow：捕获实际 CLI 命令，并用 materialized effective mission 调普通 runner。

禁止修改 frozen ZIP、tracked extracted tree 或生产 tree。若公开测试扩展面无法观察所需事实，先记录 instrumentation gap；任何临时 source overlay 必须在 ADR/owner 批准后才可使用，且记录 overlay diff/hash，不能冒充未修改 Legacy 的 canonical trace。

## 7. Trace 语义

raw trace 至少使用以下稳定概念字段；具体 schema 名称待第 3.1 节决策：

- `oracle_id`、`capture_run_id`、`rerun_index`；
- `step_index`、`t_k`、可选 `rk_stage`；
- `event_kind`：publish-start/end、phase-invoke、candidate-start/end、commit、record、termination-evaluate、terminal-observation、ordinary-replay；
- stable probe/system/semantic-field identity；
- pre/post state hash 或明确数值字段；
- source input/artifact ref；
- monotonic event sequence 只表达该进程内观察顺序，不冒充 wall-clock time。

跨进程 SimFlow 不使用本地 event sequence 比较全局顺序；它以命令、输入语义 hash、exit、effective mission 与产物 lineage 建立因果链。

## 8. 确定性与派生纪律

1. 每条 oracle 在同一隔离输入上连续运行两次；
2. raw trace 去除前必须声明的非语义字段后，生成 normalized semantic trace；默认不允许任意忽略字段；
3. raw 与 normalized 两者都保留并分别 hash；
4. expected facts 只能由版本固定的派生器从 raw evidence 生成，或由 reviewer 明确手工签署；
5. rerun semantic hash 不同即失败，不可取平均或更新 golden；
6. tolerance 只作用于声明的数值字段，不作用于 event identity、顺序、source/input hash 或分类；
7. Legacy 与未来 target 的输出分别运行，再由 comparator 读取；target 不 link/call Legacy。

## 9. 当前直接失败检查

`ORACLE-YYZ-SYNC-03` 的直接失败用例会在 position candidate 完成前提交 mass；Python evaluator 和 C++ probe 都拒绝该 journal，并证明错误路径得到 `position=8`。`ORACLE-YYZ-PUBLISH-01` 的直接失败用例在 publish 内把 committed altitude 增加 `1 m`，C++ probe 拒绝该状态变化，Python comparator 同时核对失败事实。`ORACLE-YYZ-PHASE-02` 拒绝 process/output 交换和重复 input phase。`ORACLE-YYZ-GROUP-04` 拒绝得到 `position=10` 的 split snapshot closure、未注册 member 和重复 scope ownership。`ORACLE-YYZ-CSV-05` 接受列置换后的语义等价 dataset，并拒绝缺失 t0、错位 `t_k`、陈旧发布态和重复必要表头。重复 id、跨平台聚合 hash、通用 classification completeness 等检查留到出现当前 consumer 或直接回归后再增加。

## 10. 退出检查

- 七个 oracle id 均有输入、观察、expected、tolerance、处置、mapping 和 integrity 设计；
- preserved science/time/transaction facts 与 accidental structure 逐事实分离；
- source archive + 五个关键 entry 的 identity 可独立复核；
- 现有 test tolerance 被标记为 Legacy seed，没有越权冻结 target scientific policy；
- schema 缺口、owner 和 ADR gate 显式保留；
- canonical Legacy capture 环境与 target 平台证据分离，MSVC gap 没有被掩盖；
- capture 仅发生于隔离 build workspace，产品 tree 与冻结 Legacy 零修改；
- 前五个切片包含 early-commit、publish-time mutation、phase swap/duplicate、split closure/membership，以及 CSV temporal/identity 直接失败用例；其余失败检查随具体 oracle consumer 增加；
- target 尚不存在的对象显式为 pending，没有 vacuous pass；
- `R0-LEG-001` 已完成，`R0-LEG-002` 保持 `in_progress`；publish 与同步提交切片的处置已接受，phase、continuous-group 与 CSV 切片仍显式待定；
- UTF-8、Markdown links、repository verification 与 `git diff --check` 通过。
