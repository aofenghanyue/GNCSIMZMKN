# YYZ FrozenInterval force/moment closure executable oracle

`REF-YYZ-FORCE-MOMENT-CLOSURE-001` is the accepted fixture-local executable
for pure body-frame force/moment closure. Every contribution carries a unique
source identity, the same body frame, configuration revision and half-open
validity interval. The closure computes:

```text
force_total_B = sum(force_B_i)
moment_total_about_CoM_B =
    sum(moment_at_application_B_i
        + r_CoM_to_application_B_i x force_B_i)
```

The bundle contains:

- a three-dimensional two-source wrench case with every lever-arm
  intermediate;
- a reversed contribution-order equivalence check;
- a FrozenInterval total that feeds the accepted rigid-body equations and
  produces a short analytic constant-acceleration trajectory;
- explicit separation of body-force closure from inertial gravity;
- direct rejection of duplicate source, frame mismatch, configuration
  revision mismatch, interval mismatch and non-finite force;
- reversed application-vector, pre-transported application moment and
  gravity-double-count mutations.

The Python implementation uses only standard-library `decimal`. The C++17
probe independently implements closure, minimal rigid-body RK4 and all failure
paths. No product or Legacy code is linked.

Run the direct checks through CTest:

```powershell
ctest --preset dev -R "^r0.yyz-force-moment-closure" --output-on-failure
```

This fixture does not select canonical aero, propulsion, mass, configuration
or gravity models. `CandidateState` and `AlgebraicSolve` closure remain outside
the accepted scope.
