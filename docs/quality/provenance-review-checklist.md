# Provenance review checklist

此清单用于第三方内容准入、Artifact 晋升和外部导出。任一 required 项未知时结论为 `hold`，不能用 reviewer 备注绕过机器门禁。

## Intake identity

- [ ] item id、category、scope、purpose、owner role 与目标 consumer 明确；
- [ ] category、classification 与 integrity kind 属于当前封闭治理词表，且 category/classification 配对合法；
- [ ] publisher/author、主来源 URL、version/tag/commit/date 已固定；
- [ ] 原始 archive/file bytes 与 SHA-256 已记录并复核；
- [ ] mirror、下载命令、访问日期和 upstream identity 可追踪；
- [ ] repository presence 明确为 tracked、embedded archive 或 external-untracked。

## License and rights

- [ ] copyright/ownership 或数据 permission evidence 已保存；
- [ ] 原始许可证文本及其 hash 已保存，SPDX expression 与逐文件/逐组件 scope 已审查；
- [ ] `NONE` 只表示未发现信息，`NOASSERTION` 只表示未得出结论；
- [ ] `LicenseRef-*` 具有完整文本、stable id、holder 和适用 scope；
- [ ] 修改、链接、runtime、模板、生成输出和嵌入内容的义务已分类；
- [ ] compatibility、NOTICE、attribution、source offer、relink、share-alike、专利和商标要求已闭合。

## Data, model and media

- [ ] consent、采集来源、用途、受众、地域和保留/删除范围明确；
- [ ] PII、secrets、客户/项目标识、内部路径/URL 与受限技术信息扫描通过；
- [ ] 模型权重、训练数据、字体、图片、地图、论文附录与 benchmark 数据均单独登记；
- [ ] conversion/preparation 参数、工具、人工修订和输出 hash 已进入 lineage。
- [ ] scientific claim 具有逐 artifact 的适用域、单位、frame、time、reference/oracle 与独立性依据；聚合 inventory 或同源实现输出不能替代 Scientific Authority 审查。
- [ ] executable scientific fixture 由 `gnczmkn.scientific-context/1` sidecar 绑定，claim refs 与 manifest/fact evidence refs 双向闭合；
- [ ] repository-tracked 科学来源记录 raw SHA-256 与 byte length；normalized hash 同时登记 raw hash 和 canonicalizer identity；
- [ ] implementation independence 与 scientific source independence 分开记录，确认状态附依据，未确认项保留 `not-claimed`；
- [ ] scientific-context 的权利条目、逐 artifact lineage 和外部分发状态与 provenance inventory 一致，且 runtime/public consumer 为零。

## Repository admission

- [ ] dependency layer 符合架构边界，新生产/公共依赖具有 ADR；
- [ ] vendored 文件带逐文件声明或受审查 annotation，未删除上游 notices；
- [ ] lockfile/SBOM、构建与测试证据覆盖精确版本；
- [ ] inventory、policy、documentation 和 failure tests 同步更新；
- [ ] generated evidence 具有至少一个已解析 lineage parent、非空完整性 identity，并保持所有未闭合上游的外部分发阻断；
- [ ] Legacy 仍为 read-only/evidence-only，未进入产品 include/link/runtime 路径。

## External export

- [ ] 另行接受且明确选择 repository distribution license 的决定，以及 Product Owner/Architecture Lead 批准均已记录；
- [ ] export manifest 逐文件闭合 owner、source、license、classification、integrity 和 lineage；
- [ ] `NOASSERTION`、Legacy、过期 exception 和未审查 `LicenseRef-*` 项为零；
- [ ] notices、许可证文本、source offer、relink material 和 attribution 已放入正确位置；
- [ ] destination、audience、access、retention、withdrawal 和 incident contact 明确；
- [ ] approval record 与 export receipt 固定 commit、manifest hash、approver 和时间。

## Review result

- Item/export id：
- Decision：`approve | hold | reject`
- Product Owner：
- Architecture Lead：
- Specialist reviewers：
- Evidence refs：
- Exceptions and expiry：
- Receipt ref：
