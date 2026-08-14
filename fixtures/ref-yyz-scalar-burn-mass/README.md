# YYZ scalar-burn MassState reference

`REF-YYZ-SCALAR-BURN-MASS-001` implements the accepted fixture-local
`MODEL-YYZ-SCALAR-BURN-CONSTANT-GEOMETRY-001` choice. A nonnegative scalar
fuel-consumption rate is integrated over a half-open tick interval and
subtracted from committed mass. The candidate must remain positive. Its CoM
point coordinate and every entry of its full inertia tensor are copied exactly
from the committed state.

The committed sample remains visible to interval consumers. At the closing
tick, the complete candidate becomes the next committed MassState and projects
to the next MassProperties sample. The bundle exposes Closure and rigid-core
consumer values on both sides of the commit, including the expected change in
specific force and the exact continuity of application geometry and rotational
dynamics.

An 80-digit Decimal implementation and an independent C++17 probe cover a
positive burn with products of inertia, a zero-flow boundary, interval
partition equivalence, ten direct input failures and four semantic mutations.
Fuel geometry, CoM/inertia motion, dry mass, depletion handling, configuration
changes, canonical assets and product contracts remain outside this dedicated
model.

Run the cross-tool oracle through CTest test
`r0.yyz-scalar-burn-mass.oracle` after configuring and building a preset.
