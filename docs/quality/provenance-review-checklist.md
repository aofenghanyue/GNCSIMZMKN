# Provenance review checklist

只在新的第三方内容、外部数据、binary/archive 或真实分发候选出现时使用此清单。当前 consumer、目标路径和交付形式必须先明确。

## 内容准入

- [ ] source URL、version/tag/commit、获取日期与可获得的原始 hash 已固定；
- [ ] 作者/权利方、原始许可证或数据权限证据及其逐文件/逐组件范围已确认；
- [ ] 修改、链接、runtime、代码生成、模型推理或数据转换方式已写清；
- [ ] notice、attribution、source offer、relink、用途、地域、保留和敏感信息义务已识别；
- [ ] 当前 consumer、仓库路径、架构依赖层和 provenance parent 已明确；
- [ ] vendored 原始 notices 和许可证文本保持完整；
- [ ] Legacy 内容仍位于 `reference/legacy/`，保持只读并退出产品依赖。

## 分发候选

- [ ] 仓库所有者已确认本次分发对象和交付形式；
- [ ] 候选文件清单已排除 Legacy 和任何权利未闭合项；
- [ ] 每个第三方/数据项的结论覆盖实际 source、binary、archive 或报告形态；
- [ ] 必需的许可证文本、notice、attribution、source offer 或 relink material 已进入候选；
- [ ] 生成物的 code/data/model/tool parents 均可追踪，未解决的上游限制仍阻断外发；
- [ ] secrets、PII、内部路径/URL 和受限材料检查与候选范围一致；
- [ ] `tools/validate-license-provenance.ps1 -RequireExternalReady` 通过。

未知项保持 `NOASSERTION`，结论为 hold。需要法律解释或跨司法辖区判断时，由仓库所有者取得专业意见。
