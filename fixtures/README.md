# Reference fixtures

Fixture 是可复制、可校验、带 provenance 的规范实例。每个目录至少包含 `fixture-manifest.json`，达到 executable 后再加入输入、expected facts、失败样例和运行脚本。

当前三个起始 fixture 均为 `specification_only`：

- `ref-minimal-3dof`：最小动力学、积分与终止 oracle；
- `ref-yyz-001`：source→plan→session→observation→evidence 主链；
- `ref-cavh-formula`：复杂论文算法的公式级 reference。

禁止把蓝图中的演示数值直接升级为 golden。Scientific Authority 需要确认来源、公式和容差。
