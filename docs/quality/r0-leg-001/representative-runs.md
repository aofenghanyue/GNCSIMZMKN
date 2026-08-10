# R0-LEG-001 representative mission runs

## Conclusion

Two representative missions were configured, built, and executed from the clean mission extraction at legacy commit `a63621c368aa8e7889547689bcce9c7686b886ac`:

1. the framework geographic 3DoF baseline;
2. the project-specific YYZ Cartesian 6DoF mission.

Both runner invocations returned exit code 0, closed their CSV sinks, wrote summaries, finalized the simulator, and produced hash-recorded artifacts.

The exact absolute commands and working directories are preserved in [commands.txt](runs/20260809T081000Z/commands.txt) and [commands.json](runs/20260809T081000Z/commands.json).

## Geographic 3DoF baseline

### Build and run

```text
cmake -S <clean-mission-source> -B <baseline-build> -G "MinGW Makefiles" \
  -DBUILD_TESTS=OFF \
  -DGNC_ACTIVE_PROJECT=example_05_ideal_3dof_geographic_baseline \
  -DCMAKE_BUILD_TYPE=Release
cmake --build <baseline-build> --target gnc_sim --parallel 4
<baseline-build>/bin/gnc_sim.exe \
  --config user/example_05_ideal_3dof_geographic_baseline/config/mission.json
```

The runner used the clean extracted source root as its working directory.

| Fact | Result |
| --- | --- |
| Configure / build / run exit codes | `0 / 0 / 0` |
| Runtime nodes | 25 |
| Integrator | RK4 |
| Simulation interval | `t=0` through `t=0.1 s` |
| Termination | `time limit reached` at step 1 |
| CSV rows / columns | `2 / 127` |
| CSV SHA-256 | `8f62c3f9f8c2f06a2d5e9baa9f61b42fad21fd8c683cf44005cb8cc8a655ee37` |
| Summary SHA-256 | `a8dd56c3bca29cbaad8b67c39be8d667d9240f2d1e450abdf2f6678645390a6c` |

Captured outputs:

- [ideal_3dof_geographic_baseline.csv](runs/20260809T081000Z/artifacts/example_05_ideal_3dof_geographic_baseline/ideal_3dof_geographic_baseline.csv)
- [summary.txt](runs/20260809T081000Z/artifacts/example_05_ideal_3dof_geographic_baseline/summary.txt)
- [runner log](runs/20260809T081000Z/logs/12-baseline-run.log)

The recorded summary reports final altitude `999.999697324 m`, final speed `250.000000041 m/s`, and final mass `100 kg`.

## YYZ Cartesian 6DoF

### Build and run

```text
cmake -S <clean-mission-source> -B <yyz-build> -G "MinGW Makefiles" \
  -DBUILD_TESTS=OFF \
  -DGNC_ACTIVE_PROJECT=yyz_cartesian_6dof_framework_9 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build <yyz-build> --target gnc_sim --parallel 4
<yyz-build>/bin/gnc_sim.exe \
  --config user/yyz_cartesian_6dof_framework_9/config/mission.json
```

The runner used the same clean extracted source root and a separate build directory.

| Fact | Result |
| --- | --- |
| Configure / build / run exit codes | `0 / 0 / 0` |
| Runtime nodes | 24 |
| Integrator | RK4 |
| Simulation interval | `t=0` through `t=2 s` |
| Termination | `max_time_s` at step 100 |
| CSV rows / columns | `101 / 202` |
| CSV SHA-256 | `fe8b60dffd65635d9a7d330f1d7a20dda2e0666ecc56f39711d5db1e68eec0e2` |
| Summary SHA-256 | `4a57ee8f72698b61ec35eb39ea98254cc586b07365752c44612137ced0caf577` |

Captured outputs:

- [trajectory_nominal.csv](runs/20260809T081000Z/artifacts/yyz_cartesian_6dof_framework_9/2026-08-09_161447/trajectory_nominal.csv)
- [summary.txt](runs/20260809T081000Z/artifacts/yyz_cartesian_6dof_framework_9/2026-08-09_161447/summary.txt)
- [runner log](runs/20260809T081000Z/logs/22-yyz-run.log)

The recorded summary reports final altitude `1020.39291059 m`, final speed `257.136370413 m/s`, final mass `119.6 kg`, maximum dynamic pressure `34876.3595524 Pa`, and minimum target range `988.648047664 m`.

## Artifact integrity

All captured outputs and built executables are listed in [artifact-hashes.sha256](runs/20260809T081000Z/artifact-hashes.sha256). A post-run independent hash pass recalculated every listed SHA-256 value successfully.

These files are legacy comparison evidence. Scientific migration still requires provenance, applicable-domain statements, independent oracles, and new model identities.
