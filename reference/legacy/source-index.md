# Legacy source index

归档解包前缀为 `GNCZMKN-legacy-a63621c/`。运行 `tools/extract-legacy-reference.ps1` 后，可以在 `reference/legacy/extracted/GNCZMKN-legacy-a63621c/` 查看下列文件。

## Current execution chain

| Topic | Archive path |
| --- | --- |
| Runner | `src/runner.cpp` |
| Build use case | `framework/include/gnc/core/simulation_builder.hpp` |
| Mission assembly | `framework/include/gnc/core/mission_assembler.hpp` |
| Validation | `framework/include/gnc/core/validation_pipeline.hpp` |
| Legacy simulator | `framework/include/gnc/core/simulator.hpp` |
| Factory | `framework/include/gnc/core/node_factory.hpp` |
| Assembly context | `framework/include/gnc/core/assembly_context.hpp` |
| Config manager | `framework/include/gnc/core/config_manager.hpp` |
| Math | `framework/include/gnc/common/math/` |
| Libraries | `framework/include/gnc/libraries/` |
| Interfaces | `framework/include/gnc/interfaces/` |

## Scientific and vertical references

| Topic | Archive path |
| --- | --- |
| YYZ project | `user/yyz_cartesian_6dof_framework_9/` |
| CAVH project | `user/example_08_cavh_geographic_3dof_custom/` |
| 3DoF baseline | `user/example_05_ideal_3dof_geographic_baseline/` |
| Cartesian 3DoF baseline | `user/example_06_ideal_cartesian_3dof_baseline/` |
| Cartesian 6DoF baseline | `user/example_07_ideal_cartesian_6dof_baseline/` |

## Runtime behavior tests

| Claim | Archive path |
| --- | --- |
| Publish/record/stop | `tests/test_publish_semantics.cpp` |
| Shared RK candidate state | `tests/test_continuous_group.cpp` |
| Mission/rate/config errors | `tests/test_mission_contract.cpp`, `tests/test_strict_config.cpp` |
| SimFlow boundary | `tests/test_simflow_materializer.cpp`, `tests/test_simflow_runner.cpp` |
| CAVH formulas | `tests/test_cavh_glide_range_guidance.cpp`, `tests/test_cavh_programmed_aoa_guidance.cpp` |
| Architecture facts | `tests/test_architecture_guards.cpp` |

## Legacy design notes

Archive 中的 `design-notes/` 对应 Git HEAD。新仓库 `design-notes/gnczmkn-architecture-roadmap/` 保存导入时较新的工作树蓝图，团队以新仓库副本为目标架构权威。
