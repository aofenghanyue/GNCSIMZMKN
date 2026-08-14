# YYZ MassProperties projection executable oracle

`REF-YYZ-MASS-PROPERTIES-001` implements the projection-only portion of the
accepted sampled Mass path:

```text
MassPropertiesSample@t_k = project(explicit committed MassState_k)
r_CoM_to_application_B =
    r_body_origin_to_application_B - r_body_origin_to_CoM_B
lever_arm_moment_B = r_CoM_to_application_B x force_B
specific_force_B = force_B / mass
angular_momentum_B = inertia_about_CoM_B * omega_B
angular_acceleration_B = inertia_about_CoM_B^-1 *
    (moment_about_CoM_B - omega_B x angular_momentum_B)
```

The committed state owns positive mass, the body-frame point coordinate of
the center of mass, and the full finite symmetric-positive-definite inertia
tensor about that center of mass. Projection copies those values with exact
owner, point, frame, clock, tick and configuration identity. Closure derives
its center-of-mass-to-application vector from the two point coordinates. The
rigid-core consumer preserves products of inertia.

A pending scalar mass candidate remains candidate-only throughout the current
`FrozenInterval`. The fixture projects it only from an explicitly supplied
next committed state at the declared next tick. The next state's CoM and
inertia are supplied independently, so this bundle makes no evolution claim
for either quantity.

The 80-digit Decimal reference and independent C++17 probe cover two direct
consumer cases, a common body-origin translation equivalence, strict input
domains and focused early-visibility, CoM-offset and inertia-diagonalization
failures. Neither implementation imports product or Legacy code.

Run the direct checks through CTest:

```powershell
ctest --preset dev -R "^r0.yyz-mass-properties" --output-on-failure
```

Fuel, ablation, dry-mass, CoM/inertia evolution, configuration jumps,
parallel-axis transfers, canonical vehicle assets and product contracts remain
outside this projection-only slice.
