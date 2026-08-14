# REF-MINIMAL-3DOF-001 当前实现

- 任务：`R0-SCI-002`
- fixture：`fixtures/ref-minimal-3dof/`
- 独立 oracle：`oracles/ref-minimal-3dof/reference.json`
- 数值 probe：`gnc_minimal_3dof_probe`

## 模型

bundle 使用一个 fixture-local 三轴平移动力学模型：

```text
dr/dt = v
dv/dt = acceleration - drag_rate * v
```

位置、速度和加速度均在
`frame.fixture.minimal3dof.inertial@1` 中表达，单位分别为 `m`、`m/s`
和 `m/s^2`。时间单位为 `s`，固定步长时间由 integer tick 计算。

`drag_rate = 0` 使用匀加速闭式解。`drag_rate > 0` 使用线性阻力闭式解：

```text
v_inf = acceleration / drag_rate
v(t) = v_inf + (v0 - v_inf) * exp(-drag_rate * t)
r(t) = r0 + v_inf * t
       + (v0 - v_inf) * (1 - exp(-drag_rate * t)) / drag_rate
```

## 独立实现

`tools/minimal_3dof_reference.py` 只使用 CPython 标准库，以 50 位
`decimal` 运算生成闭式轨迹。生成结果保存在
`oracles/ref-minimal-3dof/reference.json`。

`tests/minimal_3dof.cpp` 只使用 C++17 标准库，独立实现模型 RHS、classical
RK4、ExactGrid 步进、committed-tick 终止和 candidate/commit 记录。该 target
不链接产品模块或 Legacy。

比较器按 case id、tick、position 和 velocity 等语义字段对齐结果。JSON
成员顺序、C++ 内存布局、Legacy Node 数量和 CSV 列顺序均不参与判定。

## 可执行案例

| Case | 直接结果 |
| --- | --- |
| `CASE-MIN3D-CONSTANT-ACCELERATION` | 保存 tick 0..4 的闭式轨迹并逐状态比较 |
| `CASE-MIN3D-LINEAR-DRAG-CONVERGENCE` | 对 `dt = 0.8..0.05 s` 检查误差严格下降和观测阶 |
| `CASE-MIN3D-EXACT-GRID-TERMINATION` | 在 committed tick 10 首次满足 `position.z <= 0 m` |
| `CASE-MIN3D-RK-STAGE-DOMAIN-FAILURE` | k2 在 `t = 0.75 s` 失败，candidate 丢弃，tick 1 保持提交 |

输入失败覆盖 zero `dt`、负 duration、非整网格 duration、负 drag rate 和
非有限 state。

## 运行

```powershell
cmake --preset dev
cmake --build --preset dev --target gnc_minimal_3dof_probe
ctest --preset dev -R "^r0.minimal-3dof" --output-on-failure
```

当前 bundle 是 R0 validation spike，不定义 R1～R3 产品 API。ADR-0006 的
科学约定仍等待仓库所有者接受；该依赖关闭前，`R0-SCI-002` 保持活动状态。
