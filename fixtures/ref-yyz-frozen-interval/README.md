# YYZ FrozenInterval composed reference

`REF-YYZ-FROZEN-INTERVAL-001` composes the accepted fixture-local uniform
environment, air-data, aerodynamic dimensionalization, propulsion response,
current MassProperties, force/moment Closure and rigid-body profiles at one
sample boundary. The accepted inputs stay fixed over `[0,1)`, and the C++17
probe advances the coupled rigid state with one classical RK4 step.

The case is deliberately analytic. Identity attitude, zero angular rate and
zero closed moment keep the held body force fixed in inertial coordinates, so
the independent 80-digit Decimal reference evaluates the exact
constant-acceleration state at tick 1. Aerodynamic drag is `-61.25 N`, thrust
is `100 N`, the two off-center propulsion moment terms cancel exactly, and the
closed force is `[38.75,0,0] N`. The current interval uses `100 kg`; the
`99.95 kg` candidate remains hidden until the next boundary.

The bundle checks quaternion-sign equivalence, eight cross-component or input
domain failures, and three composition mutations: adding wind, exposing the
mass candidate early, and transporting the propulsion moment twice. Canonical
assets, coefficient lookup, command mapping, guidance/control, committed
center-of-mass or inertia evolution, product termination metrics and R1-R8
runtime contracts remain outside this fixture.

Run the cross-tool oracle through CTest test
`r0.yyz-frozen-interval.oracle` after configuring and building a preset.
