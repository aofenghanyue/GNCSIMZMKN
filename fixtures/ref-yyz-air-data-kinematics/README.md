# YYZ supplied air-data kinematics executable oracle

`REF-YYZ-AIR-DATA-KINEMATICS-001` executes the accepted fixture-local
kinematics that turns same-boundary Truth, WindQuery and AtmosphereQuery
samples into supplied air data. The body axes are right-handed
`x-forward/y-right/z-down`, and `q_I_B` is the passive Hamilton transform
defined by ADR-0007.

```text
v_rel_I = v_vehicle_I - v_airmass_I
pure(v_rel_B) = q_I_B * pure(v_rel_I) * inverse(q_I_B)
V = sqrt(u^2 + v^2 + w^2)
alpha = atan2(w, u)
beta = atan2(v, sqrt(u^2 + w^2))
qbar = 0.5 * rho * V^2
Mach = V / a
```

The cases cover wind subtraction, a non-identity passive rotation, rearward
flow, a tiny positive speed and a sub-one-metre-per-second speed of sound.
Quaternion sign and common-velocity invariance are executable. Nine invalid
inputs exercise frame/time identity, finite values, quaternion policy,
atmosphere domains and the two speed singularities. Four mutations detect
wind addition, reversed quaternion direction and the Legacy angle/sound-speed
clamps.

The Python reference evaluates square roots and angles with 80-digit
standard-library `decimal`, including an independent Machin-formula value of
pi. The C++17 probe independently implements the same accepted equations and
hard-codes its physical inputs. Neither executable imports product or Legacy
code.

Run the direct checks through CTest:

```powershell
ctest --preset dev -R "^r0.yyz-air-data-kinematics" --output-on-failure
```

This fixture does not define canonical atmosphere, wind, sensor, aerodynamic
validity or product contracts. Rearward flow is valid for the supplied
kinematics; a later aerodynamic model can declare a narrower domain.
