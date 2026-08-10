# R0-LEG-001 test report

## Conclusion

The frozen legacy archive builds successfully from two matching clean extractions on the recorded host. The clean CTest run reports **25/25 passed** in 8.98 seconds. This differs from the imported existing-tree report, which recorded 23/25 with two output-path failures. Both historically path-sensitive test executables also return exit code 0 when launched directly from the clean extracted source root.

Task acceptance is satisfied with one documented baseline gap, `LEG-GAP-02`. The gap records that the two imported failures do not reproduce under this clean run. No legacy source or mission file was changed.

## Input identity and clean extraction

| Fact | Observed value |
| --- | --- |
| Legacy commit | `a63621c368aa8e7889547689bcce9c7686b886ac` |
| Archive bytes | `990450` |
| Archive SHA-256 | `2159a324fd897e4bd508c140a36c9165d744e4e4e61861c5b568201707f988e5` |
| Files per extraction | `391` |
| Bytes per extracted tree | `2708191` |
| Test extraction digest | `0fd87fdfcc4bc2634669196ba70b11985de161095f7b820df0f46ac5ad2fa598` |
| Mission extraction digest | `0fd87fdfcc4bc2634669196ba70b11985de161095f7b820df0f46ac5ad2fa598` |

The digest is the SHA-256 of each sorted clean-extraction file/hash list. The two lists are byte-identical:

- [test extraction file hashes](runs/20260809T081000Z/test-clean-files.sha256)
- [mission extraction file hashes](runs/20260809T081000Z/mission-clean-files.sha256)
- [archive verification log](runs/20260809T081000Z/logs/01-archive-verification.log)
- [extraction log](runs/20260809T081000Z/logs/02-extraction.log)

## Toolchain and configuration

| Tool/fact | Observed value |
| --- | --- |
| OS | Windows NT `10.0.26100.0`, AMD64 |
| PowerShell | Desktop `5.1.26100.6584` |
| CMake / CTest | `4.0.3` |
| C++ compiler | MSYS2 GNU `15.1.0` |
| Generator | `MinGW Makefiles` |
| GNU Make | `4.4.1` |
| Build type | `Release` |
| Eigen package | vcpkg `x64-windows/share/eigen3` |
| Parallel build jobs | `4` |

The exact host facts are preserved in the [environment manifest](runs/20260809T081000Z/environment-manifest.json). The exact configure command enabled:

```text
BUILD_TESTS=ON
GNC_BUILD_EXAMPLE_TESTS=ON
GNC_BUILD_PROJECT_TESTS=OFF
GNC_BUILD_ARCHITECTURE_GUARDS=ON
```

This configuration registers the same 25-test inventory described by the imported report: 18 core tests, 6 example tests, and 1 architecture guard.

## Build result

| Stage | Exit code | Duration |
| --- | ---: | ---: |
| Configure | `0` | 2.632 s |
| Build | `0` | 331.896 s |

The compiler emitted legacy warning diagnostics, mainly unused parameters. The warnings did not fail the build. Full evidence:

- [configure log](runs/20260809T081000Z/logs/03-configure.log)
- [build log](runs/20260809T081000Z/logs/04-build.log)
- [recorded commands and exit codes](runs/20260809T081000Z/commands.txt)

## CTest result

| Result | Imported existing-tree report | Clean archive run |
| --- | ---: | ---: |
| Configured tests | 25 | 25 |
| Passed | 23 | 25 |
| Failed | 2 | 0 |
| Aggregate exit code | nonzero | `0` |
| Total test time | 7.52 s | 8.98 s |

All preserved high-value tests passed, including publish semantics, continuous grouping, strict config, SimFlow materialization/runner, and the architecture guard. The project CAVH tests remain outside this exact 25-test imported configuration, matching `GNC_BUILD_PROJECT_TESTS=OFF`.

Raw evidence:

- [CTest output](runs/20260809T081000Z/logs/05-ctest.log)
- [CTest LastTest log](runs/20260809T081000Z/logs/05-ctest-lasttest.log)

## Direct reruns of imported path-sensitive tests

Both tests were launched by absolute executable path with the clean extracted source root as the working directory.

| Test | Exit code | Duration |
| --- | ---: | ---: |
| `test_ideal_cartesian_3dof_mission` | `0` | 0.125 s |
| `test_ideal_cartesian_6dof_mission` | `0` | 0.123 s |

Raw evidence:

- [Cartesian 3DoF direct run](runs/20260809T081000Z/logs/06-direct-test_ideal_cartesian_3dof_mission.log)
- [Cartesian 6DoF direct run](runs/20260809T081000Z/logs/06-direct-test_ideal_cartesian_6dof_mission.log)

## Documented gap

`LEG-GAP-02` classifies the only observed environment/baseline difference:

- the imported report came from an existing `build-mingw` tree and recorded two relative-output failures;
- the clean archive on the recorded CMake 4.0.3 / GNU 15.1.0 host passes all 25 tests;
- direct source-root reruns also pass;
- the frozen source remains unchanged;
- the available evidence does not isolate the historical build-tree condition that produced the earlier failures.

The machine-readable record is [environment-gaps.json](runs/20260809T081000Z/environment-gaps.json).

## Evidence boundary

This report proves build and runtime reproducibility for the frozen archive on the captured environment. It does not qualify the legacy models as independent scientific oracles, and it does not authorize legacy runtime code to enter the greenfield build graph.
