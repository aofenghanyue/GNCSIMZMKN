# Framework

`framework/include/gnc/` 承载稳定架构分区。Bootstrap 阶段只建立依赖方向和最小 version header；新增类型需要对应 backlog task、架构引用和测试。

首版采用 header-oriented 组织以缩短启动周期。编译库、生成代码或稳定 ABI 需要性能、构建或语言绑定证据，并通过 ADR 引入。
