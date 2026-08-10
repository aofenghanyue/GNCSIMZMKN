# Reference fixtures

Fixture 是可复制、可校验、带 provenance 的规范实例。每个目录至少包含 `fixture-manifest.json`，并在 `specification_only` 阶段登记计划验证的 expected facts 与 tolerance policy。达到 executable 后再加入权威输入、机器可比 expected values、失败样例、hash 和运行脚本。

当前三个起始 fixture 均为 `specification_only`：

- `ref-minimal-3dof`：最小动力学、积分与终止 oracle；
- `ref-yyz-001`：source→plan→session→observation→evidence 主链；
- `ref-cavh-formula`：复杂论文算法的公式级 reference。

禁止把蓝图中的演示数值直接升级为 golden。Scientific Authority 需要确认来源、公式和容差。
