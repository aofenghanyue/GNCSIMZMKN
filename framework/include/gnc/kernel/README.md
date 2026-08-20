# kernel

职责：Session lifecycle、compiled regions、state/output/control stores、CycleFrame、StepTransaction、integration scope、commit 和 backend execution。

允许依赖：foundation、contracts，以及后续抽出的 plan-runtime contract。

禁止依赖：Compiler 实现、Mission Source、Catalog lookup、具体领域 package、Workflow、文件 sink 和前端。

R2 只在 Contracts 中形成 immutable、process-local `ExecutionPlanImage`，不创建 Kernel object。目标 R3 Kernel 从通过 G3 的 Image 物化 PreparedModel/Bound handles、workspace、RuntimeCell、state/output stores 与 compiled region executor，并在 Session 内调用 projection/query/closure/component/derivative entries、暂存 candidate 和执行原子 commit；Kernel 不重新读取 source、查询 Catalog 或选择 package implementation。当前 review Image 尚未 exact-link package-owned RuntimeCellFactory 与 invocation-result writer/binder，因此还不是该物化入口，Kernel 不得用领域/type switch临时补齐。
