# Reference fixtures

Fixture 是可复制、可校验、带 provenance 的规范实例。每个目录至少包含 `fixture-manifest.json`，并在 `specification_only` 阶段登记计划验证的 expected facts 与 tolerance policy。达到 executable 后加入权威输入、机器可比 expected values、失败样例和运行脚本。

当前两个场景 fixture 保持 `specification_only`：

- `ref-yyz-001`：source→plan→session→observation→evidence 主链；
- `ref-cavh-formula`：复杂论文算法的公式级 reference。

`ref-scientific-conventions` 已达到 `executable`，冻结 SI、frame、整数 tick 与被动 Hamilton 四元数约定，并由隔离 C++17 property spike 和独立 Python 标准库实现交叉验证。

`ref-minimal-3dof` 已达到 `executable`，包含显式初值、高精度闭式轨迹、独立 C++17 RK4 probe、收敛检查、committed-tick 终止和 candidate 丢弃失败用例。

禁止把蓝图中的演示数值直接升级为 golden。Scientific Authority 需要确认来源、公式和容差。
