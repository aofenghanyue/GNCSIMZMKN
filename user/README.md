# User workspace

`user/<project>/` 保存高变化研究代码、配置、资产和验证。项目能力先在这里经历真实成功与失败案例，再申请进入稳定 package 或 framework。

建议结构：

```text
user/<project>/
  contracts/
  algorithms/
  components/
  assets/
  config/
  verification/
  README.md
```

`user/outputs/` 默认不进入版本控制。需要长期保存的结果应提升为 fixture 或 Artifact bundle，并记录 provenance 与 hash。
