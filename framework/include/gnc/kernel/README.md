# kernel

职责：Session lifecycle、compiled regions、state/output/control stores、CycleFrame、StepTransaction、integration scope、commit 和 backend execution。

允许依赖：foundation、contracts，以及后续抽出的 plan-runtime contract。

禁止依赖：Compiler 实现、Mission Source、Catalog lookup、具体领域 package、Workflow、文件 sink 和前端。

R2 只在 Contracts 中形成 immutable、process-local `ExecutionPlanImage`，不创建 Kernel object。当前 review Image 已 exact-link package-owned typed RuntimeCellFactory、formal-output result binder 与各 numeric dependency handle，但不恢复或调用 entry。目标 R3 在 G3 通过后由 package/generated composition 物化 PreparedModel/Bound handles、workspace、RuntimeCell、state/output stores 与 compiled region executor，并在 Session 内执行 projection/query/closure/component/derivative、暂存 candidate 和原子 commit；Kernel 不重新读取 source、查询 Catalog、选择 package implementation，或用领域/model/type switch补齐静态选择。
