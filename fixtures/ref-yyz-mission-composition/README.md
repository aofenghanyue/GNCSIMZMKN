# YYZ fixture-local mission composition reference

`REF-YYZ-MISSION-COMPOSITION-001` is the first single-entry execution bundle
over the current YYZ scientific slices. It resolves every declared component
binding against the repository cases and stored oracle identities, then uses
the causal output of `REF-YYZ-FROZEN-INTERVAL-001` for the numerical step.

The opening committed sample has `100 kg` mass. Environment, air-data,
trilinear aero lookup, dimensionalization, propulsion, MassProperties,
Closure and the rigid-body core produce the tick-1 rigid candidate while a
`99.95 kg` mass candidate remains pending. Both candidates enter one atomic
closing commit. Metrics and inclusive AtGrid predicates then read the closing
committed sample, select the higher-priority downrange completion, seal the
terminal observation and freeze the mission result.

`REF-YYZ-TWO-INTERVAL-MASS-COMMIT-001` remains the executable authority for
the candidate → atomic commit → next consumer time relation. Its supplied
`240 N` trajectory is intentionally kept separate from the lookup-composed
force in this bundle. `REF-YYZ-RUN-EVALUATION-001` supplies the accepted
committed-boundary evaluation semantics; this bundle applies them to the
lookup-composed trajectory.

The Python resolver reads the declared repository dependencies and composes
their semantic outputs. The independent C++17 probe recomputes air-data,
trilinear coefficients, forces, mass transition, constant-acceleration
trajectory, metrics, termination and result ordering. Canonical mission
source, Compiler/Session contracts, guidance/control and durable evidence
remain outside this R0 fixture.

After configuring and building a preset, run CTest test
`r0.yyz-mission-composition.oracle`.
