# R0-SPEC-001 acceptance and failure plan

## Scope

This work package defines R0 governance schemas, examples, and a repeatable
validation command for fixtures, oracles, and `PlanProofRecord` evidence. The
artifacts must not create or implement an R1-R8 compiler, runtime, Session,
plugin, Python API, frontend, or compatibility layer.

## Acceptance evidence

- Each schema has an explicit version and stable schema identity.
- Valid fixture, oracle, and proof examples pass the repository validation
  command.
- The command is reproducible from a clean repository checkout and is included
  in `tools/bootstrap.ps1` or another bootstrap-reached conformance suite.
- Every scientific fixture or oracle requires identity and provenance.
- Every oracle requires tolerance and expected facts.
- Every proof record exposes the seven blueprint proof classes without
  introducing production compiler behavior.
- A conformance report records the commands, results, and reviewed decisions.

## Required failure paths

- Missing identity is rejected.
- Missing provenance is rejected.
- Missing tolerance is rejected for an oracle.
- Missing expected facts is rejected for an oracle.
- Unknown schema version or unknown closed-enum value is rejected.
- Unknown fixture/oracle status, oracle level, expected-fact comparison class,
  proof class, proof result, and premise kind each fail against the complete
  registry in ADR-0009. Fixture authority must resolve to a repository role id.
- A malformed `PlanProofRecord`, including an invalid proof class, is rejected.
- Cross-document references that cannot resolve within the supplied R0 bundle
  are rejected when the schema contract declares them resolvable.
- Any attempt to make production targets consume these R0 evidence schemas is
  rejected by a repository guard.
- Duplicate fixture, oracle, fact, or proof identities are rejected within the
  supplied validation set.
- `Rejected` and `DeferredUnsupported` proof results without a diagnostic are
  rejected; either result carrying a lowered operator is also rejected.
- `Proven` proof results without a lowered operator are rejected.
- Duplicate JSON object keys, non-finite JSON numbers, unknown object members,
  empty required lists, and malformed stable identifiers are rejected.
- A validation schema using a keyword outside the task-local supported subset
  fails closed instead of silently weakening conformance.
- Repository-relative source references that escape the repository, use an
  absolute filesystem path, or resolve to a missing target are rejected. A
  valid local URI fragment is removed only after the repository target is
  identified.
- Windows drive-prefixed references, including drive-relative forms, and
  `file:` URIs are rejected. External non-file URIs remain opaque provenance
  locations and are not treated as local paths.
- `repo://` scheme matching is case-insensitive and always enters local file
  closure; malformed reserved `repo:` syntax and directory targets are rejected.
- Proof prerequisite self-reference and multi-record cycles are rejected.
- Fixture `open_tasks` ids must resolve to the backlog, and repository-local
  oracle `artifact_refs` must resolve without escaping the workspace. Expected
  fact `field_refs` remain explicitly opaque R0 semantic ids.
- Every existing `fixtures/**/fixture-manifest.json` and
  `oracles/**/oracle-manifest.json` is inventoried. A `/1` document must conform
  to the reviewed v1 schema; an incomplete bootstrap placeholder must use the
  explicit placeholder `/0` version and carry a gate-blocking classification.

## Evidence matrix

The committed case manifest must enumerate every JSON document under the
task-local `valid/` and `invalid/` directories. Each negative case names the
diagnostic fragment that demonstrates the intended failure path. The validator
also checks that the three schemas keep their reviewed `$id`, version constant,
R0 stage, and `Fixture` maturity metadata. A committed invalid-schema sample
proves unsupported keywords fail closed. The existing-manifest classification
must exactly cover the repository inventory and cannot classify an invalid `/1`
document as a placeholder.

The task validator's production boundary scan covers root CMake and production
C/C++/CMake files under `apps/`, `framework/`, `adapters/`, `packages/`,
`user/`, and `cmake/`. In-memory sentinels must hit every forbidden schema path,
schema URN, v1 instance-version literal, placeholder `/0` literal, and validator
entry token before the live scan can report zero.

## Decision gates

A schema shape, stable ID, versioning rule, reference rule, or codec choice is
recorded in an ADR proposal before the corresponding implementation proceeds.
Architecture Lead owns the schema disposition; Validation Lead independently
reviews the positive and negative conformance evidence.
