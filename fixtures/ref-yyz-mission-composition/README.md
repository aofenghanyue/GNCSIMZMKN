# YYZ fixture-local mission composition reference

`REF-YYZ-MISSION-COMPOSITION-001` is the single-entry execution bundle over
the current YYZ scientific slices. It resolves every declared component
binding against the repository cases and stored oracle identities, then uses
the accepted formulas of `REF-YYZ-FROZEN-INTERVAL-001` for two causal steps.

The opening committed sample has `100 kg` mass. Environment, air-data,
trilinear aero lookup, dimensionalization, propulsion, MassProperties,
Closure and the rigid-body core produce the tick-1 rigid candidate while a
`99.95 kg` mass candidate remains pending. Both candidates enter one atomic
commit. Tick 1 then re-evaluates the full sampled chain from that committed
rigid state and mass, produces a new force/moment closure, advances the full
attitude/rate RK4 state and atomically commits `99.90 kg` with the tick-2 rigid
candidate. Metrics and inclusive AtGrid predicates read all three committed
samples, select the higher-priority downrange completion at tick 2, seal the
terminal observation and freeze the mission result.

`REF-YYZ-TWO-INTERVAL-MASS-COMMIT-001` remains the executable authority for
the candidate → atomic commit → next consumer time relation. Its supplied
`240 N` trajectory is intentionally kept separate from the lookup-composed
force in this bundle. `REF-YYZ-RUN-EVALUATION-001` supplies the accepted
committed-boundary evaluation semantics; this bundle applies them to the
lookup-composed trajectory.

The Python resolver reads the declared repository dependencies and recomputes
the second interval with 80-digit Decimal arithmetic. The independent C++17
probe recomputes both intervals, including air-data, trilinear coefficients,
forces, moments, mass transitions and full attitude/rate RK4. A 1/2/4/8
substep series demonstrates fourth-order self-convergence for the rotating
second interval. Canonical mission source, Compiler/Session contracts,
guidance/control and durable evidence remain outside this R0 fixture.

After configuring and building a preset, run CTest test
`r0.yyz-mission-composition.oracle`.
