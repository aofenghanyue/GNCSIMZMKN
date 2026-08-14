# Reference fixtures

Fixture 是可复制、可校验、带 provenance 的规范实例。每个目录至少包含 `fixture-manifest.json`，并在 `specification_only` 阶段登记计划验证的 expected facts 与 tolerance policy。达到 executable 后加入权威输入、机器可比 expected values、失败样例和运行脚本。

当前两个场景 fixture 保持 `specification_only`：

- `ref-yyz-001`：source→plan→session→observation→evidence 主链；
- `ref-cavh-formula`：复杂论文算法的公式级 reference。

`ref-scientific-conventions` 已达到 `executable`，冻结 SI、frame、整数 tick 与被动 Hamilton 四元数约定，并由隔离 C++17 property spike 和独立 Python 标准库实现交叉验证。

`ref-minimal-3dof` 已达到 `executable`，包含显式初值、高精度闭式轨迹、独立 C++17 RK4 probe、收敛检查、committed-tick 终止和 candidate 丢弃失败用例。

`ref-legacy-sync-commit` 已达到 `executable`，将 `ORACLE-YYZ-SYNC-03` 的冻结来源、独立 Decimal 结果、C++17 candidate/commit journal 和 early-commit 失败用例连成切片；已接受的处置保留 candidate barrier 与 committed-`t_k` 读取，并退出 Legacy 实现形状。

`ref-legacy-publish` 处于 `capturing`，交叉验证 publish 前后状态恒等、truth 边界时间、解析轨迹与 publish-time mutation 失败；Preserve/Retire 处置等待仓库所有者确认。

`ref-legacy-phase` 处于 `capturing`，包含冻结 Legacy 外部 harness 的两份原始七阶段 trace、无 Legacy 依赖的独立 C++17 调度 probe，以及 phase swap 和 duplicate 失败用例；宏 phase 顺序的处置等待仓库所有者确认。

禁止把蓝图中的演示数值直接升级为 golden。Scientific Authority 需要确认来源、公式和容差。
