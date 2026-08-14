# Legacy SimFlow materialization and replay reference

`REF-LEGACY-SIMFLOW-001` captures the frozen boundary between batch case generation and ordinary single-mission execution. A two-row matrix contains `hot` and `cold`; the minimal capture selects `hot`, injects two numeric inputs and writes `effective_mission.json` before execution.

The frozen CLI was invoked in this order for each of two runs:

```text
gnc_sim --simflow generated-simflow.json
gnc_sim --config effective_mission.json
```

Both entrypoints succeeded. Each SimFlow run used a distinct fresh working root while retaining the same configured relative output string. Before each plain replay, the harness moved the SimFlow dataset away so `--config` had to recreate the dataset path. The four recorded datasets—two SimFlow results and two plain replays—are byte-identical in their raw capture form. Semantic comparison maps required fields by header and verifies the single row `(t=0, altitude=1000 m, vz=0 m/s, mass=100 kg)`.

The bundle separates its evidence layers:

- [`base-mission.json`](base-mission.json), [`cases.csv`](cases.csv) and [`simflow-template.json`](simflow-template.json) are the declared materialization inputs;
- [`legacy_capture.py`](legacy_capture.py) invokes only the frozen CLI and copies its raw replay artifacts;
- both effective missions, summaries, command traces and SimFlow/replay datasets pin the two observed runs;
- [`reference.json`](../../oracles/ref-legacy-simflow/reference.json) independently materializes the selected row and compares JSON/CSV semantics;
- the repository C++17 probe models the same typed mission/replay boundary without Legacy linkage.

Run the executable checks through CTest:

```powershell
ctest --preset dev -R "r0.legacy-simflow" --output-on-failure
```

The comparison rejects a missing injected input, another SimFlow invocation in place of ordinary replay, and a mismatched replay result. Legacy numeric index and case-directory names are ignored. A deterministic target `CaseId`, target case manifest and target MissionSource representation remain pending future contracts. The bundle stays `capturing` until the repository owner accepts the recommended Preserve/Retire split.
