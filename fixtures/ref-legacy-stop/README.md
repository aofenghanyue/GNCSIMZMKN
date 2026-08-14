# Legacy record-before-termination reference

`REF-LEGACY-STOP-001` captures a frozen Legacy run whose altitude predicate is already satisfied at `t0`. Initial-state recording and per-step flushing are enabled. The termination evaluator reopens the active CSV and reads the recorded state before it returns `true`.

Both isolated runs produce one semantic row:

```text
time_s = 0
altitude_m = 1000
vertical_velocity_mps = 10
```

The event order is exact:

```text
publish -> record altitude -> record vertical velocity
        -> termination evaluation sees the flushed row
        -> Legacy run completes at t0
```

The bundle separates its evidence layers:

- [`legacy_capture.cpp`](legacy_capture.cpp) uses the public frozen Legacy runtime and is compiled only in the isolated extraction environment;
- both CSV files and both JSON traces pin the observed run outputs;
- [`input.json`](input.json) pins archive, source, environment, harness, runtime-test and raw output identities;
- [`reference.json`](../../oracles/ref-legacy-stop/reference.json) defines the semantic timeline, target-pending mapping and direct failure cases;
- the repository C++17 probe models the same timeline without including or linking Legacy.

Run the executable checks through CTest:

```powershell
ctest --preset dev -R "r0.legacy-stop" --output-on-failure
```

The comparison rejects termination-before-record, a missing terminal row and post-stop advancement. Legacy free-text reason changes are ignored. Target `Observation` and `RunOutcome` artifacts remain pending future-stage implementation. The bundle stays `capturing` until the repository owner accepts the recommended Preserve/Retire split.
