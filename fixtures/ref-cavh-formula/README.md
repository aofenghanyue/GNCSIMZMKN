# CAVH formula and guidance executable oracle

`REF-CAVH-FORMULA-001` qualifies a fixture-local model named
`MODEL-CAVH-LEGACY-TRANSCRIBED-FORMULA-001`. Its authority is deliberately
narrow: the archived Legacy Eq17/Eq18 transcription is independently
implemented and checked, while equation-by-equation fidelity to the cited
paper remains unclaimed until authorized full text is available.

The executable bundle covers five layers in one test boundary:

1. the analytic positive-lift optimum of a parabolic drag polar;
2. analytic density, Mach and `CL_star` derivatives plus central-difference
   convergence ladders;
3. all Eq18 and Eq17 `A` and `B` intermediates, including vertical lift under
   bank angle;
4. TDCT error sign, raw correction and both saturation directions;
5. eleven direct domain failures and seven critical scientific mutations.

Eq17 and Eq18 have separate immutable identities. A degenerate Eq17
derivative returns `derivative-degenerate` with fallback `forbidden`. A small
formula denominator returns `formula-singularity`. The oracle never clamps a
formula denominator or silently substitutes Eq18.

The Python reference uses standard-library `decimal` at 80-digit precision.
The C++17 probe implements the model and failure paths independently. Neither
implementation imports product or Legacy code.

Run the direct checks through CTest:

```powershell
ctest --preset dev -R "^r0.cavh-formula" --output-on-failure
```

The exact citation, source-access observation and assumptions are recorded in
`source.json`. Paper-faithful reproduction, digitized aerodynamic assets,
closed-loop performance and production contracts remain outside this
qualification bundle.
