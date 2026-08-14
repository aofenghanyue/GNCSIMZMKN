# YYZ aerodynamic coefficient lookup reference

`REF-YYZ-AERO-LOOKUP-001` implements an accepted fixture-local pure query over
an immutable `Mach × alpha × beta` coefficient table. Each axis is finite and
strictly increasing. Queries inside the inclusive box use trilinear
interpolation; exact upper endpoints use the final cell with unit weight.
Queries outside the validated box fail directly. Clamp and extrapolation are
disabled.

The table carries coefficients in the existing
`[C_A,C_Y,C_N,C_l,C_m,C_n]` convention. Every successful lookup is passed
directly into the accepted dimensionalization equations, which makes sign and
reference-length regressions visible at the consumer boundary.

An 80-digit Decimal implementation and an independent C++17 probe cover an
interior query, an exact knot, the inclusive upper boundary, interpolation
order equivalence, twelve input/asset failures and four lookup mutations.
Canonical aerodynamic assets, loaders, hashes, nonzero rates or surfaces,
derivatives, extrapolation policy and product contracts remain outside this
fixture.

Run the cross-tool oracle through CTest test `r0.yyz-aero-lookup.oracle` after
configuring and building a preset.
