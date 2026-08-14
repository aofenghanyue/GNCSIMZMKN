# YYZ aerodynamic dimensionalization executable oracle

`REF-YYZ-AERO-DIMENSIONALIZATION-001` fixes the accepted fixture-local
mapping from supplied body-axis aerodynamic coefficients to a dimensional
wrench:

```text
F_B = qbar * S_ref * [-C_A, C_Y, -C_N]
M_aero_ref_B = qbar * S_ref * [b_ref*C_l, c_ref*C_m, b_ref*C_n]
M_CoM_B = M_aero_ref_B + r_CoM_to_aero_ref_B x F_B
```

The body frame is right-handed with x forward, y right and z down. Positive
`C_A` therefore produces rearward force, positive `C_Y` produces force along
body +y, and positive `C_N` produces upward force along body -z. Positive
moment coefficients follow the right-hand rule about body +x, +y and +z.

The bundle contains an asymmetric six-coefficient case, an isolated
span-versus-chord case, a valid zero-dynamic-pressure case, two factorization
equivalences, ten direct domain rejections and three critical mutations. The
output includes the exact closure contribution valid on
`[sample_tick, sample_tick + 1)`.

The Python implementation uses only standard-library `decimal`. The C++17
probe independently implements the equations and failure paths. Neither
implementation imports product or Legacy code.

Run the direct checks through CTest:

```powershell
ctest --preset dev -R "^r0.yyz-aero-dimensionalization" --output-on-failure
```

Coefficient table lookup, interpolation, coefficient-asset provenance,
canonical geometry, aerodynamic applicability and product contracts remain
outside this fixture-local slice.
