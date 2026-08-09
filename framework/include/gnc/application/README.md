# application

职责：compile/run/query/control use case、opaque handle、command admission、operation lifecycle 和跨入口 DTO。

允许依赖：Compiler、Kernel、Evidence、Workflow 的公开边界。

禁止行为：向 adapter 暴露内部指针或第二套运行语义。
