# YYZ committed-run evaluation reference

`REF-YYZ-RUN-EVALUATION-001` consumes the three committed boundary samples
from `REF-YYZ-TWO-INTERVAL-MASS-COMMIT-001`. It computes duration, downrange,
remaining mass, consumed mass, terminal speed, peak speed, maximum downrange
and minimum remaining mass without reading pending candidates or runtime state.

Two fixture-local plans exercise a complete result and a safety abort. The
complete case reaches duration and downrange thresholds together at tick 2;
the higher-priority downrange predicate supplies the primary `Complete`
decision. The abort case reaches the remaining-mass floor at tick 1 and maps
`Abort` to `Terminated`. Both use inclusive AtGrid relations.

The terminal committed sample is included in a sealed terminal observation.
The compact result freezes afterwards and carries the structured decision,
metric summary and final boundary. An 80-digit Decimal implementation and an
independent C++17 probe check predicate-order equivalence, twelve invalid
inputs and five targeted mutations covering candidate visibility, threshold
inclusivity, priority, terminal ordering and post-terminal evaluation.

Canonical thresholds, product evaluator/Observation/RunOutcome contracts,
continuous event location, lifecycle and evidence durability remain outside
this R0 fixture.

Run the cross-tool oracle through CTest test
`r0.yyz-run-evaluation.oracle` after configuring and building a preset.
