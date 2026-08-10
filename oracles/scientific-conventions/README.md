# Scientific convention oracle

- Oracle set id: `ORACLE-R0-SCI-CONVENTIONS-001`
- Reference implementation id: `gnczmkn.scientific-conventions.python/1`
- C++ comparison implementation id: `gnczmkn.scientific-conventions.cpp-test/1`
- Scope: R0 mathematical, SI, frame, time, and quaternion conventions
- Applicable domain: finite binary64 values; proper 3D rotations; declared SI conversions; finite typed simulation-time values

## Authority and provenance

The scientific choices are defined by ADR-0005 through ADR-0008 and the target architecture, 03 §6-§9 and §22. SI names and semantics trace to the [BIPM SI Brochure](https://www.bipm.org/en/si-brochure-9). Rotation and time cases are synthetic analytic fixtures. They contain no migrated legacy formula, parameter, API, or asset.

`reference.py` requires Python 3.10 or newer, uses only the Python standard library, and shares no implementation code with the C++ property executable. Both implementations read the same checked-in input cases and compute their results independently.

## Inputs and outputs

- `cases.csv` contains axis-angle rotations, vectors, and translations. `passive_angle_rad` means the angle of `R_to_from`; ADR-0008 therefore constructs `q_to_from = [cos(theta/2), -axis*sin(theta/2)]`.
- `reference.py --self-test` checks analytic properties and mandatory failures.
- `scientific_conventions_properties --emit-cross-tool cases.csv` emits the C++ results as CSV.
- `cross_tool_check.py` compares both tools, records hashes and environment, and writes a machine-readable report.
- `evidence/cross-tool-report.json` is the retained run used for R0 review.

The default comparison tolerance is:

```text
abs(candidate - reference)
<= 1e-12 + 1e-12 * max(abs(candidate), abs(reference))
```

Each implementation also checks unit quaternion norm, rotation determinant, orthogonality, matrix/quaternion agreement, and inverse round trip. Domain failures cover non-finite numbers, unknown units, below-zero Kelvin, invalid matrices, clock mismatches, reversed validity intervals, malformed/zero quaternions, rejected non-unit quaternions, and incomplete Euler metadata.

## Reproduction

After configuring and building the `dev` preset, run:

```powershell
ctest --preset dev -R scientific_conventions
python oracles/scientific-conventions/cross_tool_check.py `
  --cpp build/dev/gnc_scientific_conventions_properties.exe `
  --cases oracles/scientific-conventions/cases.csv `
  --cpp-source tests/scientific_conventions_properties.cpp `
  --cmake-cache build/dev/CMakeCache.txt `
  --approval-status approved `
  --reviewer-assignee codex-r0-architecture `
  --reviewed-at 2026-08-09 `
  --output oracles/scientific-conventions/evidence/cross-tool-report.json
```

On generators without the `.exe` suffix, pass the executable path produced by CMake.

## Maturity and review

The oracle set is `executable`. Architecture Lead approved ADR-0005 through ADR-0008 and the retained report on 2026-08-09. This oracle freezes R0 evidence only; it does not mark any production foundation contract as Implemented.
