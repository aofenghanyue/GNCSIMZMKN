# Legacy seven-phase invocation reference

`REF-LEGACY-PHASE-001` captures the frozen single-tick order of seven discrete macro phases. One stateless probe is registered for every phase. Registration order is deliberately scrambled, and priorities are chosen so that a global priority sort would also produce a different sequence.

The actual Legacy harness observes:

```text
environment -> perturbation -> input -> process -> output -> interaction -> evaluation
```

The bundle keeps the capture and target-independent checks separate:

- [`legacy_capture.cpp`](legacy_capture.cpp) uses the public Legacy test surface and is compiled only inside a clean extraction workspace;
- [`legacy-run-1.json`](legacy-run-1.json) and [`legacy-run-2.json`](legacy-run-2.json) are the raw outputs captured with w64devkit GCC 16.2.0;
- [`input.json`](input.json) pins the archive, source entries, canonical environment evidence, harness and both trace byte identities;
- [`reference.json`](../../oracles/ref-legacy-phase/reference.json) defines exact event fields, normalization, direct failures and the pending disposition;
- the repository C++17 probe independently schedules the same semantic phases without including or linking Legacy.

Run the repository checks through CTest:

```powershell
ctest --preset dev -R "r0.legacy-phase" --output-on-failure
```

The main CMake graph validates the frozen traces and independent probe. Regenerating the raw Legacy traces requires the fixed `R0-LEG-001` extraction environment and the standalone capture harness. The repository owner accepted the Preserve/Retire split, so the bundle is `executable`.
