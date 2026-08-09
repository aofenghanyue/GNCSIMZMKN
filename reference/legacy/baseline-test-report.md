# Legacy baseline test report

- Capture date: 2026-08-09
- Source commit: `a63621c368aa8e7889547689bcce9c7686b886ac`
- Existing build tree: `build-mingw`
- Command: `ctest --test-dir build-mingw --output-on-failure`

## Result

- Configured tests: 25
- Passed through CTest: 23
- Failed through CTest: 2
- Total CTest time: 7.52 s

失败项：

- `test_ideal_cartesian_3dof_mission`
- `test_ideal_cartesian_6dof_mission`

两项都完成了仿真，随后因相对输出路径解析差异无法读取 CSV。分别从旧仓库根目录直接运行对应 executable 时，退出码均为 0。

## Interpretation

该结果只能证明导入时构建的大部分测试健康，并暴露两个工作目录敏感测试。R0-LEG-001 仍需从干净 archive 重建，记录工具链、CTest working directory 和全部输出 hash。当前报告不能替代 G1 oracle。

## Preserved high-value tests

- `test_publish_semantics`
- `test_continuous_group`
- `test_strict_config`
- `test_simflow_materializer`
- `test_simflow_runner`
- `test_cavh_glide_range_guidance`
- `test_cavh_programmed_aoa_guidance`
- `test_architecture_guards`
