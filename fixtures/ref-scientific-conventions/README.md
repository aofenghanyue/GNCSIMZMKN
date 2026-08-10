# Scientific convention fixture

`REF-SCIENTIFIC-CONVENTIONS-001` freezes the R0 scientific language before production math, frame and time types exist.

- `fixture-manifest.json` records provenance, authority and evidence requirements.
- `conventions.json` is the fixture-level executable profile for ADR-0006 and ADR-0007.
- `cases.json` contains inputs, expected observations and comparison tolerances.

The C++ test under `tests/` is an isolated validation spike. The Python reference uses only the CPython standard library and does not import project or Legacy implementations. Neither file is a runtime consumer of this fixture.
