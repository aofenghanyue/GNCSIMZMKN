# Legacy continuous-group reference

`REF-LEGACY-CONTINUOUS-GROUP-001` captures the smallest frozen case that distinguishes a shared RK candidate closure from separately integrated continuous systems.

The case starts with `mass=10 kg` and `position=0 m`, uses `mass'=-2 kg/s` and `position'=candidate mass`, and advances one `dt=1 s` RK4 step. The four joint candidate states are:

```text
stage 1: t=0.0, mass=10, position=0.0
stage 2: t=0.5, mass=9,  position=5.0
stage 3: t=0.5, mass=9,  position=4.5
stage 4: t=1.0, mass=8,  position=9.0
```

One group commit produces `mass=8` and `position=9`. The split snapshot control uses committed `t_k` mass for the entire position step and produces `position=10`, so the failure is directly observable.

Member semantics are bound by the stable `mass` and `position` identities. Reversing declaration and packed storage order still produces the same four stages and final state; the oracle therefore does not preserve Legacy manual vector packing order.

The bundle separates actual Legacy capture and target-independent checks:

- [`legacy_capture.cpp`](legacy_capture.cpp) instruments the public Legacy group/test surface in a clean extraction workspace;
- [`legacy-run-1.json`](legacy-run-1.json) and [`legacy-run-2.json`](legacy-run-2.json) contain the raw stage, commit and membership results;
- [`input.json`](input.json) pins the archive, group/RK4/simulator/test entries, canonical environment, harness and trace bytes;
- [`reference.json`](../../oracles/ref-legacy-continuous-group/reference.json) contains the 50-digit Decimal stages, split-closure control, exact membership outcomes and pending disposition;
- the repository C++17 probe independently evaluates the joint RK4 state and membership rules without including or linking Legacy.

Run the repository checks through CTest:

```powershell
ctest --preset dev -R "r0.legacy-continuous-group" --output-on-failure
```

The main CMake graph validates frozen traces and independent references. Raw Legacy regeneration uses the fixed `R0-LEG-001` extraction environment and standalone harness. The bundle remains `capturing` until the repository owner accepts the recommended Preserve/Retire split.
