# Review checklist

## Architecture

- [ ] AuthorityDomain、owner 和 boundary 明确。
- [ ] 输入与输出使用 typed contract。
- [ ] sample/effective/commit time 明确。
- [ ] failure、rollback、effect 和 evidence 闭合。
- [ ] 依赖方向符合 ADR-0003。
- [ ] Legacy production dependency 为零。

## Science

- [ ] 公式、单位、frame、方向和适用域明确。
- [ ] 数值策略、容差和 fallback 明确。
- [ ] 独立 reference 与中间量可复现。
- [ ] 新旧差异完成分类。

## Delivery

- [ ] Task acceptance 全部满足。
- [ ] 成功、失败和边界测试通过。
- [ ] Schema、文档、Artifact 和 hash 已更新。
- [ ] `tools/verify-repository.ps1` 通过。
