# ADR-0009: R0 fixture, oracle, and plan-proof schema contract

- Status: Accepted
- Date: 2026-08-09
- Owners: Architecture Lead
- Reviewers: Architecture Lead, Validation Lead
- Review evidence: R0-SPEC-001 independent read-only approvals, 2026-08-09
- Related tasks: R0-SPEC-001, R0-GATE-001
- Architecture references: 05 sections 10.6 and 13, 00A section 3.4, roadmap R0 sections 2.2, 2.4, and 2.8, ADR-0004, ADR-0010, ADR-0011

## Context

R0 needs machine-checkable evidence envelopes before scientific reference
bundles and compiler proof fixtures can be reviewed consistently. The existing
bootstrap schemas and manifests are placeholders: fixture identity has no
provenance link, oracle claims have no explicit expected-fact list, and the
proof schema lacks a schema version and uses a result spelling that differs from
the architecture definition. Several existing manifests already carry a `/1`
string even though they cannot satisfy the reviewed v1 contract, so their gate
status also needs an explicit machine-readable disposition.

This decision covers governance and evidence documents only. It does not define
a Mission Source, Compiler output, runtime payload, Artifact API, public C++
type, or compatibility reader.

## Decision

### Schema identities and versions

Keep three UTF-8 JSON Schema Draft 2020-12 documents at their existing paths:

| Document | Stable schema identity | Instance version |
| --- | --- | --- |
| fixture manifest | `urn:gnczmkn:schema:r0:fixture-manifest:1` | `gnczmkn.fixture-manifest/1` |
| oracle manifest | `urn:gnczmkn:schema:r0:oracle-manifest:1` | `gnczmkn.oracle-manifest/1` |
| plan proof record | `urn:gnczmkn:schema:r0:plan-proof-record:1` | `gnczmkn.plan-proof-record/1` |

Every schema carries `x-stage: R0` and `x-maturity: Fixture`. A document with an
unknown instance version fails. Any compatible addition to version 1 must
remain accepted by the version-1 validator; a required-field, identity,
meaning, closed-enum, or reference-rule change receives a new schema identity
and instance version. R0 provides no legacy reader.

Existing fixture and oracle identities such as `REF-YYZ-001` and
`ORACLE-YYZ-PUBLISH-01` remain stable. Display text and paths do not become
identity. The complete v1 identity grammar is:

| Identity | Case-sensitive grammar |
| --- | --- |
| fixture | `^REF-[A-Z0-9]+(?:-[A-Z0-9]+)+$` |
| oracle set and oracle item | `^ORACLE-[A-Z0-9]+(?:-[A-Z0-9]+)+$` |
| expected fact | `^FACT-[A-Z0-9]+(?:-[A-Z0-9]+)+$` |
| proof | `^proof:[a-z0-9][a-z0-9._-]*(?::[a-z0-9][a-z0-9._-]*){2,}$` |
| proof premise | `^[a-z][a-z0-9_.-]*$` |
| proof assertion code | `^GNC\.PLAN\.[A-Z0-9_.-]+$` |
| proof-generating pass | `^[a-z][a-z0-9.-]*@[1-9][0-9]*$` |

A proof identity keeps the architecture form
`proof:<namespace>:<subject>:<suffix>`. This ADR does not select a hash or
prescribe how a future Compiler derives the suffix.

### Fixture manifest

A fixture manifest requires its stable identity, status, purpose, authority,
provenance references, source references, required artifacts, expected facts,
oracle references, and acceptance statements. Each expected fact has its own
stable fact identity, statement, and closed comparison class. The document is
closed to unknown members.

The fixture status registry is exactly `specification_only`, `capturing`,
`executable`, `qualified`, and `retired`. `authority` is an exact repository
role id from `docs/team/role-assignments.json`; the JSON Schema checks role-id
syntax and the conformance validator resolves the value against that registry.

### Oracle manifest

An oracle set requires its stable identity, status, provenance references, and
one or more oracle entries. Every entry requires an identity, level, maturity
status, claim, expected facts, independent-reference description, tolerance
policy, and source references. The tolerance policy may be concise text during
R0; it must be present and non-empty. A later numeric tolerance container is a
separate contract decision when a consumer needs it.

The oracle status registry is exactly `planned`, `capturing`, `executable`,
`qualified`, and `retired`. The oracle level registry is exactly `math`,
`numerical`, `model`, `component`, `closure`, `mission`, and `workflow`.

Fixture and oracle expected facts share the complete comparison registry
`exact`, `numeric`, `ordered`, `set_equality`, and `predicate`. `exact` compares
the full declared fact, `numeric` delegates numeric bounds to the oracle's
tolerance policy, `ordered` compares sequence, `set_equality` ignores member
order, and `predicate` evaluates the named pass/fail condition represented by
the fact statement and oracle evidence. R0 records the class and evidence; it
does not create a runtime comparison engine.

Every expected fact has a case-sensitive stable identity matching
`FACT-[A-Z0-9]+(?:-[A-Z0-9]+)+`. Renaming that identity is a new fact or an
explicit migration; editing display text cannot silently change which fact an
oracle or fixture claims. The validator rejects duplicate fact identities in a
supplied validation set.

Three optional v1 reference fields have narrow R0 semantics:

- fixture `open_tasks` contains exact task ids from `docs/tasks/backlog.json`
  whose status is `planned`, `ready`, `in_progress`, `blocked`, or `review`.
  A `done` task is rejected. Omission or an empty array means the fixture
  declares no currently open task. This field reports governance work and does
  not alter fixture identity or scientific meaning.
- expected-fact `field_refs` lists the stable semantic field identities observed
  by that fact. R0 checks non-empty, unique strings only. These values are opaque
  because Mission, observation, and dataset field registries remain locked to
  their owning stages; this proposal grants no path, schema, or runtime lookup
  meaning.
- oracle `artifact_refs` lists supporting evidence locations. Repository-relative
  and `repo://` values must resolve to a file after fragment removal. `artifact:`,
  `https:`, and other non-file external URIs remain opaque references governed
  by provenance and later Artifact contracts. An empty array is permitted when
  a planned or executable oracle has no committed artifact yet.

### PlanProofRecord

The R0 proof fixture follows the blueprint fields and adds an explicit instance
schema version. `proof_kind` is the closed seven-value set `Identity`,
`Ownership`, `Causality`, `TimeLifecycle`, `StateTransition`,
`ResourceEffect`, and `Evidence`. `result` is the closed set `Proven`,
`Rejected`, and `DeferredUnsupported`.

A proven record has at least one lowered operator and no diagnostic. A rejected
or deferred-unsupported record has at least one diagnostic and no lowered
operator.

The normative field sketch in 05 section 10.6 declares `premises[]`, while the
illustrative YYZ YAML in 00A section 3.4 renders premises as a key/value map.
The v1 machine schema follows the array declaration and uses a closed tagged
union:

- `Fact` carries `premise_id` and one JSON `value`;
- `ProofReference` carries `premise_id` and one `proof_ref`.

The YYZ map maps losslessly by emitting one `Fact` per key and placing its value
in `value`; for example `base_rate_hz: 100` becomes
`{kind: Fact, premise_id: base_rate_hz, value: 100}`. Array order has no semantic
authority in R0. Premise identities must be unique within a record. A future
proof hash/canonicalization ADR must choose ordering and byte serialization
before deriving `proof_id`; this proposal makes no such choice. Proof-reference
premises resolve by exact identity, cannot refer to the containing proof, and
cannot form a cycle.

### Reference closure

`provenance_refs` resolve by exact `record_id` against the supplied provenance
register. Fixture `oracle_refs` resolve by exact oracle identity within the
supplied validation set. `ProofReference` premises resolve by exact proof
identity, cannot refer to the containing proof, and cannot form a cycle.
Repository-relative source references are checked for an existing file after an
optional fragment is removed. URI scheme matching is case-insensitive, so any
case spelling of `repo://` enters the same local closure check. The canonical
written form remains lowercase. A reserved `repo:` value without `//` is
malformed and fails. The validator reports unresolved references before a
bundle can become gate evidence.

Absolute paths, any Windows drive prefix (including drive-relative syntax),
UNC-style paths, and `file:` URIs are forbidden because they can escape the
reviewed workspace. The validator repeats this full syntax check after removing
a `repo://` wrapper, so the repository scheme cannot hide any forbidden local
form. Non-file external URIs such as `https:` remain opaque
source locations; this task verifies their syntax-bearing presence and leaves
network availability and rights review to the provenance policy.

### Existing placeholder disposition

The validator inventories every fixture and oracle manifest in the repository.
Any document that claims a v1 instance version must validate against v1 and
close its declared references. An incomplete bootstrap document is migrated to
the explicit non-contract version `gnczmkn.fixture-manifest.placeholder/0` or
`gnczmkn.oracle-manifest.placeholder/0` and appears in the task-owned gate
classification with its owner task and factual blockers. Placeholder entries do
not receive invented provenance, expected facts, or tolerance values.

The executable scientific-conventions oracle already has a reviewed provenance
record and is a v1 migration candidate. Its content hash participates in
R0-SCI-001 evidence, so the coordinator serializes that migration and refreshes
the corresponding hash and provenance record. A `/1` label cannot be waived by
the classification file.

### Conformance command

Use `tools/verify-r0-spec-001.ps1` as the repeatable command. It invokes a
task-local Python standard-library validator that implements only the JSON
Schema keywords used by these three schemas and fails closed on any unhandled
keyword. It also applies the cross-document reference rules, checks duplicate
identities, scans the existing-manifest inventory, and executes every committed
valid and invalid example from the case manifest. No third-party package is
introduced.

The same command scans root CMake plus production C/C++ and CMake files under
`apps/`, `framework/`, `adapters/`, `packages/`, `user/`, and `cmake/`. It
rejects references to the three schema paths, their three stable URNs, the three
v1 instance-version literals, either placeholder `/0` literal, or either
task-validator entry point. In-memory sentinels exercise every forbidden token
on each run. Test, documentation, evidence, schema, fixture, oracle, and tool
trees remain outside this production-consumer scan.

The validator and schemas remain test/evidence tooling. Production CMake
targets, include paths, framework modules, adapters, and packages cannot consume
them. Repository integration adds the command to the bootstrap gate and adds a
guard for this boundary after review.

## Consequences

- Positive: fixture, oracle, and proof evidence fails at one deterministic
  boundary with stable diagnostics.
- Positive: expected facts, tolerance, provenance, and proof class are visible
  before later scientific or compiler work begins.
- Cost: existing placeholder manifests receive an explicit placeholder `/0`
  disposition and remain gate-blocked until their owning scientific tasks
  provide real facts and provenance.
- Cost: the task-local validator supports a deliberately small schema keyword
  subset and rejects schema growth until its conformance coverage is extended.
- Risk: the proof identity suffix remains opaque until a future hash decision;
  R0 verifies syntax and reference closure only.
- Modules kept unchanged: foundation, contracts, model_sdk, compiler, kernel,
  evidence, workflow, application, adapters, packages, and legacy reference.

## Alternatives considered

- Free-form Markdown manifests: rejected because required facts and reference
  closure would remain manual.
- Reuse the production Compiler contract during R0: deferred because R1-R8 are
  locked and no production consumer exists.
- Add a general third-party JSON Schema dependency: deferred because the three
  closed R0 schemas use a small keyword set and no dependency review exists.
- Accept unknown versions or enum values with warnings: rejected because this
  would make evidence interpretation tool-dependent.

## Verification

- All committed valid examples pass their declared schema and reference checks.
- Every required negative path has a committed invalid example and expected
  diagnostic fragment.
- Duplicate JSON keys and unsupported schema keywords fail closed.
- Premise maps from 00A have an executable array-form example, and proof
  prerequisite self-reference and multi-record cycles are rejected.
- Existing fixture/oracle manifest inventory is exact; every `/1` document
  conforms, and every placeholder uses `/0` with a gate blocker.
- The three schema identities, instance versions, stage, and maturity match this
  proposal exactly.
- `tools/bootstrap.ps1` passes after coordinator integration.
- Repository guards confirm that no production target or production source
  consumes the R0 schemas or task-local validator.
- Production guard sentinels detect every forbidden path, URN, and validator
  entry token; the live production scan reports zero references.

## Supersession rule

Reopen this decision when a production consumer is proposed, a schema requires
a new mandatory field or closed value, the reference boundary changes, a proof
identity hash is selected, or a standard validator dependency is introduced.
The superseding decision must include migration, valid and invalid fixtures,
and updated bootstrap evidence.
