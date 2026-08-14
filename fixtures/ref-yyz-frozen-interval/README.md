# YYZ FrozenInterval composed reference

`REF-YYZ-FROZEN-INTERVAL-001` composes the accepted fixture-local uniform
environment, air-data, three-axis aerodynamic lookup and dimensionalization,
propulsion response, current MassProperties, force/moment Closure and
rigid-body profiles at one sample boundary. The accepted inputs stay fixed
over `[0,1)`, and the C++17 probe advances the coupled rigid state with one
classical RK4 step.

The case is deliberately analytic. Air data produces the interior lookup
weights `[4/17,1/2,1/2]` and coefficients
`[27/850,0,0,0,-3/68,0]`. The fixture-local aerodynamic reference point uses
the exact lever `z=-25/18 m`, which balances the lookup pitch moment at the
nominal sample. Identity attitude, zero angular rate and zero closed moment
therefore keep the held body force fixed in inertial coordinates. The
independent 80-digit Decimal reference evaluates the exact
constant-acceleration state at tick 1. Aerodynamic force is
`[-6615/34,0,0] N`, thrust is `100 N`, and the closed force is
`[-3215/34,0,0] N`. The current interval uses `100 kg`; the `99.95 kg`
candidate remains hidden until the next boundary.

The bundle checks quaternion-sign equivalence, ten cross-component, lookup or
input-domain failures, and four composition mutations: adding wind, exposing
the mass candidate early, transporting the propulsion moment twice, and
replacing trilinear lookup with nearest-grid selection. Canonical aero assets,
asset schema/loaders, command mapping, guidance/control, committed
center-of-mass or inertia evolution, product termination metrics and R1-R8
runtime contracts remain outside this fixture.

Run the cross-tool oracle through CTest test
`r0.yyz-frozen-interval.oracle` after configuring and building a preset.
