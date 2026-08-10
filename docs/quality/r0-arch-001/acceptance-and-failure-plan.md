# R0-ARCH-001 acceptance and failure plan

## Scope

- Task: `R0-ARCH-001`
- Gate: `R0`
- Owner role: Architecture Lead
- Production capability change: none
- Governing proposals: `docs/adr/0010-r0-architecture-baseline-format.md` and `docs/adr/0011-r0-scientific-terminology-overlay.md`

## Required deliverables

| Deliverable | Planned path | Authoritative input |
| --- | --- | --- |
| Machine-readable terminology baseline | `specs/architecture/r0/terminology-baseline.json` | `reference-glossary.md`, ADR-0005 through ADR-0008, ADR-0011 |
| Module dependency map | `specs/architecture/r0/module-dependency-map.json` | 02 section 13, ADR-0003, root `CMakeLists.txt` |
| Legacy-to-target ownership map | `specs/architecture/r0/legacy-to-target-ownership-map.json` | reference glossary section 9, 01 section 16 |
| Repeatable conformance validator | `tools/verify-r0-arch-001.ps1` | all three baselines and their sources |
| Terminology conformance report | `docs/quality/r0-arch-001/terminology-conformance-report.md` and `.json` | validator output |

## Failure paths to prove before acceptance

| Failure ID | Injected or detected condition | Required result |
| --- | --- | --- |
| `ARCH-TERM-001` | duplicate canonical term or alias collision | validation fails and identifies the duplicate identity |
| `ARCH-TERM-002` | term, enum, key, or owner has zero or multiple registry authorities | validation fails and identifies the affected entry |
| `ARCH-TERM-003` | glossary content or extracted row changes without a reviewed baseline update | validation fails with source or baseline drift |
| `ARCH-TERM-004` | the R0 scientific term overlay, ASCII SI unit ids, time kinds, or quaternion coefficient order drift | validation fails with the missing or mismatched scientific registration |
| `ARCH-DAG-001` | undeclared module, duplicate edge, or dependency cycle | validation fails before reporting conformance |
| `ARCH-DAG-002` | `kernel` reaches `compiler`, or another declared forbidden dependency becomes reachable | validation fails with the forbidden path |
| `ARCH-DAG-003` | declared direct module edges differ from root CMake target links | validation fails with missing and unexpected edges |
| `ARCH-LEG-001` | a registered legacy concept lacks a migration entry | validation fails with the missing legacy identity |
| `ARCH-LEG-002` | a target responsibility has no unique AuthorityDomain, owner role, target module, or canonical target term | validation fails with the incomplete responsibility |
| `ARCH-LEG-003` | a target term is absent from the terminology baseline | validation fails with the unresolved target term |

## Positive acceptance evidence

1. All canonical glossary rows and retired aliases appear exactly once in the terminology baseline.
2. Selected complete shared enums, key compositions, and the four AuthorityDomains each reference one registry authority and one owner role.
3. The R0 scientific overlay registers the nine cross-book terms missing from the glossary, exact ASCII SI unit ids, five time kinds, and quaternion coefficient order without creating production types or schemas.
4. The dependency graph is acyclic, matches the root CMake direct links, and satisfies ADR-0003 forbidden reachability.
5. Every legacy identity registered in the glossary has one or more target responsibilities; every responsibility has a unique owner and canonical target vocabulary.
6. Ordinary validation and negative self-tests pass.
7. Repository verification, configure, build, CTest, and `tools/bootstrap.ps1` pass.

## Evidence capture

The conformance report records UTC time, source SHA-256 values, artifact SHA-256 values, counts, each positive check, each negative self-test, command results, known limits, and ADR status. Task completion remains subject to independent review and ADR disposition.
