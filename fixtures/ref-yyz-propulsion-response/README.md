# YYZ supplied propulsion response executable oracle

`REF-YYZ-PROPULSION-RESPONSE-001` fixes the accepted fixture-local supplied
propulsion response:

```text
force_B = thrust_magnitude * thrust_direction_B_unit
lever_arm_moment_B = r_CoM_to_application_B x force_B
moment_about_CoM_B = intrinsic_moment_at_application_B + lever_arm_moment_B

interval_duration = (valid_until_tick - valid_from_tick) * base_dt
consumed_fuel_mass = fuel_consumption_rate * interval_duration
mass_delta = -consumed_fuel_mass
mass_candidate = committed_mass + mass_delta
```

Thrust magnitude is finite and nonnegative. The direction is an explicit
finite unit free vector in the accepted right-handed
`x-forward/y-right/z-down` body frame. The response supplies the vector from
the center of mass to the application point and the intrinsic moment at that
point. The accepted Closure is the only owner of wrench transport.

`fuel_consumption_rate_kgps` is a finite nonnegative consumption magnitude.
The Mass consumer subtracts its half-open interval integral. A nonpositive
candidate produces a domain failure; no dry-mass or epsilon clamp is present.
One equivalence case partitions an interval and proves identical total
consumption and final candidate mass.

The Python implementation uses only standard-library `decimal`. The C++17
probe independently implements the response, Closure and Mass consumer links,
identity/domain failures and three focused physical mutations. Neither
implementation imports product or Legacy code.

Run the direct checks through CTest:

```powershell
ctest --preset dev -R "^r0.yyz-propulsion-response" --output-on-failure
```

Throttle and enable mapping, engine/fuel dynamics, phase logic, ablation,
fuel exhaustion, dry-mass policy, canonical engine assets, configuration
transitions and product contracts remain outside this fixture-local slice.
