# YYZ 6DoF rigid-body core executable oracle

`REF-YYZ-6DOF-CORE-001` is the accepted isolated executable for the first
`R0-SCI-003` formula slice. It evaluates a constant-mass, constant-inertia
rigid body in an inertial Cartesian frame with supplied gravity, total body
force at the center of mass, total body moment about the center of mass, and
the accepted passive Hamilton `q_I_B` convention.

The bundle contains:

- one coupled instantaneous derivative case with force transformation,
  angular momentum, gyroscopic term, net moment and quaternion derivative;
- one constant-translation analytic trajectory with ExactGrid termination;
- one principal-axis spin trajectory and an RK4 step-size convergence ladder;
- one injected RK-stage domain failure that preserves the last committed
  state;
- direct rejection cases for invalid mass, inertia, quaternion, finite-value,
  step-size and duration-grid inputs.

The Python implementation uses standard-library `decimal` arithmetic and
high-precision trigonometric series for the stored reference. The C++17 probe
implements the equations, symmetric-positive-definite inertia solve, RK4,
quaternion normalization and candidate/commit bookkeeping independently. No
product or Legacy code is linked.

Run the direct checks through CTest:

```powershell
ctest --preset dev -R "^r0.yyz-6dof-core" --output-on-failure
```

The Scientific Authority accepted this exact fixture-local model and its
normalization policy. The bundle does not select the canonical 00A or Legacy
mission, environment, aero, propulsion, guidance, control, terminal metric or
production tolerance.
