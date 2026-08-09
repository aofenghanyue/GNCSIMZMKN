# 验收矩阵

| 需求 | 首个 executable evidence | 主要 gate |
| --- | --- | --- |
| 数学与姿态可信 | quaternion/frame/unit property suite | G1 |
| 固定步长时间语义 | publish/phase/record/stop oracle | G1/G4 |
| 模型与算法解耦 | algorithm six-piece tests | G2 |
| Mission 可编译 | YYZ dry-run、proof index、negative suite | G3 |
| 整步原子提交 | failure-point transaction matrix | G4 |
| YYZ 纵向闭环 | source→plan→run→outcome evidence | G5 |
| Legacy 独立 | dependency scan 与新 runner graph | G6 |
| 数据与谱系 | CSV/MAT round-trip、RunManifest、lineage | G7 |
| 研究闭环 | DATCOM→分析→仿真→报告 bundle | G8 |
| 多 Session/Python | isolation、reset/step、vector benchmark | G9 |
| LLM/Blueprint | proposal/diff/approval/compile fixture | G10 |
| Realtime | snapshot/command/pacing consumer | G10 |

每个 evidence 必须有输入、版本、环境、hash、容差、结果和批准记录。
