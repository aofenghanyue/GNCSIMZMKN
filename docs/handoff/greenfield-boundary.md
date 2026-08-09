# Greenfield 与 legacy 边界

## 1. 仓库策略

生产框架从空骨架建立。旧 GNCZMKN 以冻结归档提供行为、模型、资产和测试参照，生命周期与新框架完全分离。

```text
legacy archive
  -> evidence extraction / comparison only
  -> fixture and oracle provenance

new framework
  -> independent source tree
  -> independent build graph
  -> independent API and schemas
```

## 2. 允许的使用方式

- 阅读旧实现以发现隐藏语义和异常路径；
- 运行旧 mission 生成 comparison evidence；
- 提取已确认的物理公式、参数和数据资产；
- 将旧测试转写为面向目标契约的 fixture；
- 在报告中引用 legacy commit、输入 hash 和输出 hash。

## 3. 禁止的使用方式

- 把 `reference/legacy/` 加入 include path、link path 或运行搜索路径；
- 让新 Session 调用旧 Simulator、NodeRegistry、provider 或 ConfigNode；
- 通过 facade、feature flag 或 dual runtime 长期保留旧路径；
- 直接复制旧模型后宣称已验证；
- 把旧 CSV 列顺序、类名或节点数量写入新稳定 API。

## 4. 科学内容迁入协议

每项迁入内容需要同时提供：

1. 来源文件、legacy commit 和原始数据 hash；
2. 公式、坐标、单位、方向、时间与适用域；
3. 新 Definition/Algorithm/Asset identity；
4. 独立 reference 或可解释的 golden intermediate values；
5. 正常、边界、失败和域外测试；
6. 与旧结果的差异分类；
7. Scientific Authority 审批记录。

## 5. 退出 legacy 的条件

某项能力具备独立 specification、oracle、fixture、源码和自动测试后，legacy source 对该能力降级为历史证据。整个旧仓库仍保留归档和 hash，不进入日常构建。
