# R0-SCI-001 acceptance and failure matrix

- Task: `R0-SCI-001`
- Owner role: Scientific Authority
- Reviewer role: Architecture Lead
- Scope: mathematical storage, SI, frame transforms, time values, and quaternion conventions
- Maturity target: R0 gate evidence; no R1-R8 runtime capability

## Failure paths fixed before implementation

| Area | Rejected input or misuse | Required executable evidence |
| --- | --- | --- |
| Numeric foundation | A non-finite scalar enters a scientific value | C++ and Python checks reject NaN and infinity before transformation or serialization |
| SI normalization | An unknown unit tag is requested | Conversion returns an explicit failure; no identity fallback is permitted |
| SI normalization | A temperature converts below absolute zero | Conversion returns an explicit domain failure |
| Frame transform | A matrix is non-orthogonal, reflective, or has a non-unit determinant | Proper-rotation validation fails |
| Frame transform | A point transform is applied as a free-vector transform | The fixture demonstrates the translation mismatch and requires distinct operations |
| Time | A time value is non-finite, a validity interval is reversed, or clock domains are mixed | Construction or comparison fails explicitly |
| Quaternion | Storage has a count other than four, contains a non-finite coefficient, or has zero norm | Deserialization/normalization fails explicitly |
| Quaternion | A non-unit quaternion reaches a rotation under `Reject` policy | Rotation fails; `NormalizeWithFlag` succeeds and records normalization |
| Euler interchange | Sequence, intrinsic/extrinsic mode, or angle unit is absent | Interchange validation fails; there is no implicit Euler default |

## Acceptance evidence planned before implementation

| Requirement | Success evidence | Property or comparison |
| --- | --- | --- |
| Mathematical direction | `scientific_conventions.properties` | Column-vector multiplication and proper-rotation invariants |
| SI convention | C++ property suite plus Python reference self-test | `km -> m`, `deg -> rad`, `degC -> K`, identity SI conversions, and failure paths |
| Frame convention | C++ property suite plus cross-tool cases | `R_to_from`, inverse, composition, point/free-vector distinction, and determinant |
| Time convention | C++ property suite plus Python reference self-test | Five semantic kinds, SI seconds, typed arithmetic, half-open validity interval, clock-domain rejection |
| Quaternion direction and multiplication | C++ property suite plus Python reference | Passive transform, Hamilton product, inverse, composition order, and `q`/`-q` equivalence |
| Quaternion serialization | C++ property suite plus cross-tool report | Exact `[w, x, y, z]` coefficient order and finite four-value validation |
| Cross-tool agreement | `scientific_conventions.cross_tool` | Standard-library Python and independent C++ implementations agree on fixed cases within declared tolerances |

## Evidence retention contract

The cross-tool report must contain the input identity and SHA-256, implementation identities, environment, tolerance policy, per-case error metrics, overall result, and approval state. The report is reproducible from the checked-in case set and test-local implementations. Build-directory output is disposable; the reviewed report under `oracles/scientific-conventions/evidence/` is the retained artifact.

## Scope guard

All executable C++ code for this work package stays under `tests/`. Python code stays under `oracles/scientific-conventions/`. The work package adds no production headers, runtime manager, Session behavior, compiler behavior, plugin mechanism, language binding, or legacy dependency.

## Recorded R0 evidence

- Evidence run date: 2026-08-09
- Retained report: `oracles/scientific-conventions/evidence/cross-tool-report.json`
- Environment: Windows 11, GCC 15.1.0, C++17 debug build, fast-math disabled, CPython 3.12.7
- Python reference: 554 property/failure checks passed
- C++ property tool: 559 property/failure checks passed
- Cross-tool comparison: 6 cases × 17 values = 102 comparisons passed
- Maximum absolute Python/C++ difference: `2.220446049250313e-16`
- Maximum fraction of the allowed tolerance: `0.00022204460492503128`
- Full repository result: `tools/bootstrap.ps1` passed, including 5/5 CTest cases, repository guards, and provenance policy conformance
- Bytecode hygiene: the cross-tool runner disables local bytecode writes; no `__pycache__` remains under the oracle directory

The retained report records input identity, environment, build configuration, source and binary hashes, tolerance formula, per-case errors, implementation self-test counts, coverage, result, and approval state.

## Frozen artifact hashes

| Artifact | SHA-256 |
| --- | --- |
| `oracles/scientific-conventions/oracle-manifest.json` | `36533390b59dac37657ae13f39759d1538e950457a4e383f48eafc62dd376a69` |
| `oracles/scientific-conventions/cases.csv` | `0f403f1f7355ce3173563c2f096c1b2b452bdc662891824eb125682dea966656` |
| `oracles/scientific-conventions/reference.py` | `7f0ac6ed9682854cce12c8b04381aed2c9f82d633884e0fdf047cb261259e05b` |
| `oracles/scientific-conventions/cross_tool_check.py` | `5893232b58b183f44b79ef8f0359089ecc79fbd5260c17f022493555241978a2` |
| `tests/scientific_conventions_properties.cpp` | `0ff18ce21b4943a5eb0fa09be5afd8be52e9fe9b3eaef630467fc8056a9f603f` |
| `oracles/scientific-conventions/evidence/cross-tool-report.json` | `ce51c79895a52df11ddece4f670d7b838458686f23b86c7224612e12461bd0af` |

## Provenance handoff

The scientific fixture is first-party and synthetic. Its applicable domain is finite proper 3D rotations, the declared SI boundary conversions, and finite typed scientific times. Units are radians, dimensionless quaternion coefficients, and the SI identifiers in ADR-0005. Frames are generic right-handed `from`/`to` frames following ADR-0006. Time follows ADR-0007. The independent basis is analytic SO(3)/Hamilton identities implemented with the Python standard library, separate from the C++ implementation. The new target identity is `ORACLE-R0-SCI-CONVENTIONS-001`.

The coordinator serialized `prov.oracle.r0-scientific-conventions@1` into
`docs/quality/provenance-register.json` under the shared-file rule. The record
contains the final hashes above, the scientific context, the independence
basis, and the Architecture Lead approval. The live provenance validator and
the complete bootstrap both pass. External sharing remains blocked by the
repository's internal-only policy and requires a separate rights decision.

## Review state and remaining decisions

Scientific Authority evidence is complete. Architecture Lead approved ADR-0005 through ADR-0008 and the retained report on 2026-08-09; the report records reviewer `codex-r0-architecture` and status `approved`. Exact JSON containers, coefficient-level quaternion hashing, runtime storage precision, UTC/leap-second handling, and production quantity/quaternion types remain outside this work package. Architecture Lead also identified missing glossary registrations for some scientific terms as a non-blocking R0-ARCH-001 integration item.

Scientific Authority independently approved the ADR-0009 v1 manifest migration
on 2026-08-09. The read-only inverse transformation exactly recovered the
original 3199-byte manifest and its frozen `eedc2ddb...` digest; the current
`36533390...` digest matches the migrated manifest, provenance register, and
this evidence table. The full review is retained in
`docs/quality/r0-spec-001/scientific-authority-migration-review.md`.
