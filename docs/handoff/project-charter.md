# 项目章程

## 1. 使命

建设一套面向制导、导航与控制研究的可信研究工作台，使模型、算法、仿真、数据、外部工具和研究证据共享一条可追踪语义主线。

## 2. 首要用户

- GNC 理论研究者；
- 动力学、气动、传感器和执行机构模型开发者；
- 仿真内核与数值算法维护者；
- 批量试验、工具链和报告开发者；
- Python、蓝图和实时前端开发者。

框架保持实验室级、自用优先。团队要优先保证科学可信、可复现、可诊断和长期可理解性。

## 3. 产品边界

首个完整目标覆盖：

- C++17 模块化单体；
- 模型与算法 package；
- Mission Source、Canonical Model Graph 和 Execution Plan；
- 确定性 Simulation Session 与事务提交；
- Observation、Artifact、lineage 和 Run Manifest；
- Workflow、Experiment、Python、受控 authoring 与软实时适配。

下列能力继续服从独立准入门：

- 动态 package ABI；
- 多机分布式执行；
- 硬实时和 HIL；
- 未知数量运行时实体；
- 步内精确 jump；
- 商业平台的租户、市场、计费和通用权限体系。

## 4. 成功标准

1. 一条 YYZ 6DoF mission 可从 source 编译为可解释 plan，并由唯一 Session 执行。
2. 正常、终止、取消和失败路径拥有唯一 state/commit/outcome。
3. 公式、模型、closure 和 mission 均有独立 oracle。
4. 数据与报告可追到 source、plan、package、asset、numerical policy 和代码版本。
5. 新算法、新数据编码、新 workflow 和新前端沿固定接缝加入，Kernel 不出现领域分支。
6. Legacy runtime 对生产构建的依赖计数保持为零。

## 5. 交付原则

- 每阶段先完成一个真实纵向 slice，再扩大能力面。
- 架构进度由可运行链路、失败证据和删除量衡量。
- Fixture、Gate、Implemented 三种成熟度必须明确区分。
- 局部便利不能越过 Plan、Commit 或 Artifact/Control 防火墙。
- 研究结论的可信度优先于功能数量和界面完成度。
