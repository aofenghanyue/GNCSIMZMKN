# kernel

职责：Session lifecycle、compiled regions、state/output/control stores、CycleFrame、StepTransaction、integration scope、commit 和 backend execution。

允许依赖：foundation、contracts，以及后续抽出的 plan-runtime contract。

禁止依赖：Compiler 实现、Mission Source、Catalog lookup、具体领域 package、Workflow、文件 sink 和前端。
