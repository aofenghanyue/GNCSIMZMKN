# YYZ two-interval mass commit reference

`REF-YYZ-TWO-INTERVAL-MASS-COMMIT-001` composes the accepted fixture-local
constant-geometry scalar-burn model with two consecutive `FrozenInterval`
rigid translations. Interval zero integrates with the committed `120 kg`,
holds the `119.95 kg` result as candidate-only, and atomically commits that
mass with the tick-1 rigid candidate. Interval one then integrates with the
newly committed `119.95 kg` and produces `119.90 kg` at tick 2.

The held body force is `[240,0,0] N`; attitude is identity and angular rate,
moment and gravity are zero. Each interval therefore has exact constant
acceleration. An 80-digit Decimal implementation composes the two analytic
segments, while the independent C++17 probe uses classical RK4. The expected
tick-2 x position is about `2.0400041684 m` and x velocity is about
`10.4000833681 m/s`.

The bundle checks one-step/two-substep RK4 equivalence, thirteen input-domain
failures, and three temporal mutations: early mass visibility, a stale mass
consumer after commit, and a stale rigid state paired with the committed mass.
Aerodynamic/environment/propulsion recomputation, fuel geometry, depletion
events, canonical assets and product runtime contracts remain outside this
fixture.

Run the cross-tool oracle through CTest test
`r0.yyz-two-interval-mass-commit.oracle` after configuring and building a
preset.
