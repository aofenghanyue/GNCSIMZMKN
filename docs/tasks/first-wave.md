# R0 推荐开发顺序

R0 采用单线推进。每次只激活一个能产生可执行结果的切片。

| 顺序 | Task | 直接结果 |
| --- | --- | --- |
| 1 | `R0-SCI-001` | 收口数学、单位、frame、时间和四元数约定，只保留需要 owner 选择的事项 |
| 2 | `R0-SCI-002` | minimal 3DoF 独立 reference、收敛检查和失败用例 |
| 3 | `R0-LEG-001` / `R0-LEG-002` | 复核 Legacy 复现并提取运行行为 oracle |
| 4 | `R0-SCI-003` | YYZ 6DoF 可执行 reference bundle |
| 5 | `R0-SCI-004` | CAVH 公式与制导 reference bundle |
| 6 | `R0-ARCH-002` | 只实现上述切片实际需要的关键架构守卫 |
| 7 | `R0-PERF-001` | 用已有可执行 workload 建立确定性与性能基线 |
| 8 | `R0-GATE-001` | 仓库所有者评审 G0/G1 并决定 R1 解锁 |

`R0-GOV-002` 需要真实权利与分发输入，保持 `planned`。它不应驱动新的审计实现；当仓库所有者准备处理外部分发时再激活。

`R0-GATE-001` 完成前，R1～R8 生产任务保持 `planned`。
