# Legacy 干净归档复现

本目录保存 `R0-LEG-001` 的受控复现记录。当前结果由 [current.json](current.json) 指向；冻结归档、第三方工具归档和输出工件均通过 SHA-256 关联，未把第三方二进制或约 90 MB 的任务输出提交到新仓库。

## 当前结论

- 冻结提交：`a63621c368aa8e7889547689bcce9c7686b886ac`。
- 冻结 ZIP：`2159a324fd897e4bd508c140a36c9165d744e4e4e61861c5b568201707f988e5`。
- 干净配置与构建：通过，C++17 / Release / GCC 16.2.0 / Ninja 1.13.2 / CMake 4.4.2 / Eigen 3.4.0。
- 测试：27/27 通过；标签分布为 core 18、example 6、project 2、architecture-guard 1。
- 历史上受工作目录影响的两个测试在干净 CTest 中通过，从 Legacy 源码根目录直接执行也均退出 0。
- 五条代表任务均连续执行两次并退出 0；CSV 原始哈希一致，summary 剔除四类运行时易变行后的哈希一致。
- CSV SHA-256 是跨干净提取复核的行为基线。summary 的 `normalized_sha256` 仍保留组件 `origin` 中的绝对源码根，只用于同一提取根内的双跑比较；跨工作区比较需先把源码根规范化为同一占位值。
- 可执行文件 SHA-256 标识单次构建工件。冻结源码通过 `__FILE__` 把绝对源码路径写入可执行文件，因此不同提取根不要求二进制哈希相等。
- 归档源码指纹前后均为 390 个文件，聚合哈希为 `ce443a79dc491326de45f4bfcbb9332ba5d2d1fcf7b69bdabaf2bd2df002feb1`。
- MSVC 19.50 能配置但不能编译 CAV-H 测试；该差异记录在 [compatibility-msvc-19.50.json](compatibility-msvc-19.50.json)，未修改冻结源码规避错误。

## 固定外部输入

复现脚本不联网，只接受以下两个官方发布归档，并在创建工作区前验证哈希：

| 输入 | 官方来源 | SHA-256 |
|---|---|---|
| w64devkit 2.9.1 x64 | [GitHub release](https://github.com/skeeto/w64devkit/releases/tag/v2.9.1) | `9208c19755cd4964b7915b9afcf02c66d493a4c870c4b3e83f6c538d9c1237a5` |
| Eigen 3.4.0 | [GitLab release](https://gitlab.com/libeigen/eigen/-/releases/3.4.0) | `eba3f3d414d2f8cba2919c78ec6daab08fc71ba2ba4ae502b7e5d4d99fc02cda` |

工具归档只解包到 `build/legacy-reproduction/runs/<RunId>/deps/`，不会安装系统包、修改全局 PATH 或进入产品构建依赖。

## 完整重跑

把两个下载归档放入忽略目录后，从新仓库根目录执行：

```powershell
powershell -NoLogo -NoProfile -ExecutionPolicy Bypass `
  -File tools/reproduce-legacy.ps1 `
  -RunId r0-leg-001-20260810-07 `
  -W64DevkitArchive build/legacy-reproduction/deps/w64devkit-x64-2.9.1.7z.exe `
  -EigenArchive build/legacy-reproduction/deps/eigen-3.4.0.zip `
  -Parallel 4
```

脚本拒绝复用已有 RunId。失败时保留阶段日志并写入 `failure.json`；成功时写入环境、测试、任务和证据索引。`user/outputs/**` 与 `test_outputs/**` 是显式生成产物根，不参与只读源码指纹；除此之外的任何文件变化都会使复现失败。

## 证据结构

- [environment-manifest.json](r0-leg-001-20260810-07/environment-manifest.json)：主机、工具链、环境变量、构建选项、源码与可执行文件哈希。
- [test-report.json](r0-leg-001-20260810-07/test-report.json)：27 项测试、标签和两个工作目录敏感测试的对照。
- [mission-report.json](r0-leg-001-20260810-07/mission-report.json)：五条任务的命令、两次运行、输出大小及原始/归一化哈希。
- [evidence-index.json](r0-leg-001-20260810-07/evidence-index.json)：32 份结构化记录与原始阶段日志的大小和 SHA-256。
- `r0-leg-001-20260810-07/logs/`：依赖准备、配置、构建、CTest、直接复跑与任务 stdout/stderr。

`tools/validate-legacy-reproduction.ps1` 会重新计算索引哈希并检查受控基线，不会重新执行耗时的 Legacy 构建。
