# Minimal 3DoF executable reference

`REF-MINIMAL-3DOF-001` exercises one fixture-local translational model:

```text
dr/dt = v
dv/dt = acceleration - drag_rate * v
```

The bundle contains four focused cases:

- a constant-acceleration trajectory with an analytic solution;
- a linear-drag trajectory used to measure classical RK4 convergence;
- an exact-grid altitude termination evaluated on committed ticks;
- an RK-stage domain failure that leaves the last committed state unchanged.

Inputs and tolerances live in `cases.json`. The CPython standard-library
reference computes closed-form trajectories with 50-digit `decimal`
arithmetic. The isolated C++17 probe implements the differential equation,
classical RK4 and candidate/commit bookkeeping. It does not link product or
Legacy code.

Run the direct checks through CTest:

```powershell
ctest --preset dev -R "^r0.minimal-3dof" --output-on-failure
```

The comparison addresses fields by semantic case and quantity identifiers.
JSON member order, C++ storage order, Legacy node count and CSV layout do not
participate in the verdict.
