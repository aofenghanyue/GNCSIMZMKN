# 架构阅读路线

目标蓝图位于 `design-notes/gnczmkn-architecture-roadmap/`。首次阅读采用分层路线，避免从完整术语表起步。

## 全员必读

1. `README.md`：主线、尺度和六次权威交接。
2. `00-vision-and-architecture-principles.md`：愿景、范围与宪章。
3. `00a-yyz-end-to-end-walkthrough.md`：数据级参考 fixture。
4. `02-layered-reference-architecture.md`：五分区、三道防火墙和扩展接缝。
5. `15-reference-vertical-designs-and-object-placement.md`：YYZ/CAVH 与压力场景。
6. `11-roadmap-overview.md`：阶段依赖与 gate。

## 按角色继续阅读

| 角色 | 分册 |
| --- | --- |
| Scientific Authority / Numerics Lead | 03、04、14、15 |
| Model SDK Lead | 04、12、13、15 |
| Compiler Lead | 02、04、05、12、14 |
| Runtime Lead | 03、06、07、12、14 |
| Evidence Lead | 07、08、09 |
| Workflow Lead | 08、09、10 |
| Application/Frontend Lead | 05、08、10 |
| Validation Lead | 00A、07、11、迁移治理分册 |

## 阅读输出

每位负责人需要提交一页范围确认，回答：

- 本模块消费什么权威输入；
- 产生什么权威输出；
- 拥有什么状态或资源；
- 时间与提交边界在哪里；
- 失败如何表达；
- 哪些模块应保持零修改；
- 首个 executable fixture 是什么。

范围确认通过后，负责人才能把相应 backlog 任务切换到 `in_progress`。
