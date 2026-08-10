# R0-LEG-001 legacy reproduction evidence

- Task: `R0-LEG-001`
- Assignee: `codex-r0-validation`
- Evidence status: `complete_with_documented_gaps`
- Backlog transition: approved for coordinator completion
- Independent reviewer: `codex-r0-science` (`scientific_authority`), approved 2026-08-09
- Frozen source commit: `a63621c368aa8e7889547689bcce9c7686b886ac`
- Frozen archive SHA-256: `2159a324fd897e4bd508c140a36c9165d744e4e4e61861c5b568201707f988e5`
- Working root: `build/r0-leg-001/`
- Evidence root: `docs/quality/r0-leg-001/`

## Scope and boundary

This work package reproduces the frozen legacy archive from a clean extraction. It records the host environment, configure/build/test commands, representative mission commands, raw command output, exit codes, output-file hashes, and every observed environment gap.

The archive and all files under `reference/legacy/` remain unchanged. Extraction, compilation, and mission execution occur under `build/r0-leg-001/`. No legacy path may enter a new-framework CMake target, include path, link path, or runtime search path. R1-R8 product capabilities remain locked.

## Failure paths fixed before execution

| ID | Failure path | Required evidence and handling |
| --- | --- | --- |
| `LEG-F01` | Archive SHA-256 differs from the manifest or checksum file | Stop before extraction; preserve expected and actual hashes plus the command exit code. |
| `LEG-F02` | Clean extraction destination already exists or the archive prefix differs | Stop before configuration; preserve the destination inventory and archive prefix observation. |
| `LEG-F03` | Required host tool or Eigen package is unavailable | Record the exact tool probe/configure failure as an environment gap; do not edit the legacy source. |
| `LEG-F04` | CMake configure or build fails | Preserve full stdout/stderr, exit code, generator, compiler identity, cache options, and the failing stage. |
| `LEG-F05` | CTest differs from the imported 23/25 baseline | Preserve CTest output and `Testing/Temporary/LastTest.log`; classify each difference, including working-directory sensitivity. |
| `LEG-F06` | A path-sensitive CTest failure cannot pass through the documented direct executable rerun | Preserve both commands, working directories, exit codes, and output paths as an unresolved environment gap. |
| `LEG-F07` | A representative mission exits unsuccessfully or omits declared CSV/summary artifacts | Preserve the mission log and output inventory; do not repair legacy runtime or mission source in this task. |
| `LEG-F08` | Generated evidence lacks a hash, command, exit code, or environment identity | Treat the reproduction as incomplete and keep the task open. |
| `LEG-F09` | New-framework bootstrap or repository boundary verification fails after evidence capture | Preserve the root verification log and report the boundary gap before handoff. |

## Acceptance result

Run `20260809T081000Z` completed all reproduction stages. The archive hash matched both recorded sources, two clean extractions produced identical 391-file tree digests, the 25-test build succeeded, CTest passed 25/25, both documented direct reruns passed, and both representative missions produced captured CSV and summary artifacts.

One baseline difference remains recorded as `LEG-GAP-02`: the imported existing-tree report showed 23/25, while this clean archive run produced 25/25. The historical condition behind the earlier path failures was not isolated. Both results remain preserved without modifying legacy source.

Key review documents:

- [human-readable test report](test-report.md)
- [representative mission report](representative-runs.md)
- [machine-readable final result](runs/20260809T081000Z/result-summary.json)
- [environment manifest](runs/20260809T081000Z/environment-manifest.json)
- [all commands and exit codes](runs/20260809T081000Z/commands.txt)
- [environment gap record](runs/20260809T081000Z/environment-gaps.json)
- [artifact hashes](runs/20260809T081000Z/artifact-hashes.sha256)

## Acceptance evidence map

| Acceptance claim | Planned authoritative evidence |
| --- | --- |
| Frozen input identity is proven before extraction | [environment manifest](runs/20260809T081000Z/environment-manifest.json), [archive verification log](runs/20260809T081000Z/logs/01-archive-verification.log) |
| Extraction is clean and repeatable | [result summary](runs/20260809T081000Z/result-summary.json), [extraction log](runs/20260809T081000Z/logs/02-extraction.log), paired clean file/hash lists |
| Legacy toolchain and build options are explicit | [environment manifest](runs/20260809T081000Z/environment-manifest.json), [commands](runs/20260809T081000Z/commands.txt), [configure log](runs/20260809T081000Z/logs/03-configure.log) |
| Imported test baseline is reproduced or every difference is explained | [test report](test-report.md), [CTest log](runs/20260809T081000Z/logs/05-ctest.log), direct-rerun logs, [gap record](runs/20260809T081000Z/environment-gaps.json) |
| Representative missions are reproducible | [representative run report](representative-runs.md), mission logs, captured outputs, and [artifact hashes](runs/20260809T081000Z/artifact-hashes.sha256) |
| All generated evidence is reviewable | This index, raw logs, machine-readable summaries, captured artifacts, and SHA-256 records under this directory |
| Greenfield boundary remains intact | [bootstrap log](runs/20260809T081000Z/logs/99-bootstrap.log): exit code 0, new-framework CTest 5/5, repository verification passed |

## Executed command sequence

1. Verify the archive against `source-manifest.json` and `legacy-source.sha256`.
2. Create a new task working root and extract the archive once.
3. Capture OS, PowerShell, CMake, CTest, compiler, generator, Ninja/Make, and Eigen discovery facts.
4. Configure all 25 legacy tests with the documented MinGW generator and all optional test groups enabled.
5. Build and run CTest from the clean tree.
6. Directly rerun the two documented working-directory-sensitive executables from the legacy source root.
7. Run the active geographic 3DoF mission and one YYZ 6DoF mission from isolated builds, then hash declared outputs.
8. Run the new repository bootstrap and record its result.

All 13 recorded legacy reproduction operations returned exit code 0. Exact paths, arguments, working directories, timestamps, durations, and log locations are stored in [commands.json](runs/20260809T081000Z/commands.json). The final greenfield bootstrap also returned exit code 0; its raw output is preserved separately because it verifies the receiving repository rather than the frozen legacy build. Generated facts do not claim stronger scientific validity than the frozen legacy evidence supports.

## Final repository verification

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/bootstrap.ps1
```

Observed result:

- new-framework configure/build: passed;
- new-framework CTest: 5/5 passed;
- repository verifier: passed;
- validated JSON files: 34;
- validated task entries: 65;
- validated Markdown files: 83;
- provenance policy conformance checks: passed;
- bootstrap exit code: 0.

See the [raw bootstrap log](runs/20260809T081000Z/logs/99-bootstrap.log).

## Independent review disposition

The Scientific Authority independently approved this evidence bundle. The
review recalculated all 38 manifest entries and all 782 per-file hashes across
the paired clean extractions, confirmed the recorded mission facts, and found
no change under `reference/legacy/`. `LEG-GAP-02` remains explicit: the imported
report recorded 23/25 while the clean run passed 25/25, and the historical
failure condition has not been isolated. This bundle is legacy behavior and
reproduction evidence; it carries no claim of independent scientific validity.

The coordinator registered the generated bundle as
`prov.evidence.r0-leg-001@20260809T081000Z` with inherited legacy restrictions
in `docs/quality/provenance-register.json`.
