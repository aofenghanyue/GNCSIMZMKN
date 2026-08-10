# `r0-leg-001-20260810-07`

状态：Passed。记录时间：2026-08-10 13:20 CST（`2026-08-10T05:20Z`）。

| 检查面 | 结果 |
|---|---:|
| 冻结源码非产物文件 | 390 / 390，指纹一致 |
| 构建目标动作 | 85，退出 0 |
| CTest | 27 / 27 |
| 直接源码根目录复跑 | 2 / 2 |
| 独立 active-project 可执行文件 | 3 / 3 |
| 代表任务 | 5 / 5，每条双跑 |
| 被索引证据 | 32 份，1,605,549 字节 |

## 代表任务基线

| 任务 | 确定性输出 | 字节 | SHA-256 |
|---|---|---:|---|
| Geographic 3DoF | `ideal_3dof_geographic_baseline.csv` | 6,856 | `8f62c3f9f8c2f06a2d5e9baa9f61b42fad21fd8c683cf44005cb8cc8a655ee37` |
| Cartesian 3DoF | `ideal_cartesian_3dof_baseline.csv` | 5,141 | `8c42c511804021536650a0f0b9c2211681b0f6ff5688734cea4a55065d3f60a7` |
| Cartesian 6DoF | `ideal_cartesian_6dof_baseline.csv` | 6,700 | `685cd0c25ce56ad0061de6e740c6e822a09059212214b2bd55b0c9e51ed9f0c0` |
| CAV-H Geographic 3DoF | `cavh_geographic_3dof_custom.csv` | 44,256,835 | `e3ada3dfdf3ef57eaacb1df59dcc9e75d94d67c0436e9ae9438f8d70d6dba6b1` |
| YYZ Cartesian 6DoF | `trajectory_nominal.csv` | 115,477 | `fe8b60dffd65635d9a7d330f1d7a20dda2e0666ecc56f39711d5db1e68eec0e2` |

summary 的原始文件包含生成时间、墙钟耗时、实时倍率和时间戳输出路径；[mission-report.json](mission-report.json) 同时保存原始哈希和剔除这四类行后的哈希，确定性判断使用后者。
