# R0-SPEC-001 conformance report

- Task: `R0-SPEC-001`
- Assignee: `codex-r0-spec`
- Evidence date: 2026-08-09
- Branch: `codex/r0-first-wave`
- Decision record: `docs/adr/0009-r0-bundle-and-plan-proof-schemas.md`
- Scope: R0 governance and evidence schemas only

## Delivered contract

Three JSON Schema Draft 2020-12 evidence contracts are accepted for R0 use:

| Contract | Stable schema identity | Instance version |
| --- | --- | --- |
| Fixture manifest | `urn:gnczmkn:schema:r0:fixture-manifest:1` | `gnczmkn.fixture-manifest/1` |
| Oracle manifest | `urn:gnczmkn:schema:r0:oracle-manifest:1` | `gnczmkn.oracle-manifest/1` |
| Plan proof record | `urn:gnczmkn:schema:r0:plan-proof-record:1` | `gnczmkn.plan-proof-record/1` |

All three carry `x-stage: R0` and `x-maturity: Fixture`. They have no production
consumer and do not implement Compiler, Session, runtime, package, Python API,
frontend, or compatibility behavior.

## Commands and results

Task-local command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/verify-r0-spec-001.ps1
```

Final result:

| Check | Result |
| --- | --- |
| Valid schema instances | 6 passed |
| Required invalid instances | 52 rejected with their declared diagnostic fragments |
| Invalid schema keyword sample | 1 rejected |
| Task-status fail-closed sentinel | passed |
| Production-boundary sentinels | 13/13 detected |
| Production C/C++/CMake scan | 4 files, 0 forbidden references |
| Existing manifest inventory | 1 conforming v1, 4 gate-blocked placeholder v0 |
| Suite failures | 0 |

Bootstrap-reached command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/bootstrap.ps1
```

Final result: exit 0. CMake configure/build passed, CTest passed 5/5,
repository verification passed, provenance policy conformance passed, and the
same R0-SPEC matrix ran from `tools/bootstrap.ps1` and passed. The repository
verification run parsed 95 JSON files, 65 task entries, and 89 Markdown files.

## Acceptance closure

| Acceptance area | Evidence |
| --- | --- |
| Stable schema identity and explicit version | Three schema `$id` and `schema_version` constants; validator identity guard |
| Fixture/oracle identity and provenance | Required fields plus missing-field and unresolved-provenance cases |
| Oracle tolerance and expected facts | Required fields plus independent missing/empty/unknown-comparison cases |
| Seven proof classes | Closed `proof_kind` registry and invalid-class case |
| Proof result semantics | Proven, Rejected, and DeferredUnsupported valid cases plus missing diagnostic/operator and forbidden operator/diagnostic cases |
| 05/00A premise shape | `premises[]` Fact/ProofReference tagged union plus the 00A guidance-rate map-to-array example |
| Reference closure | Provenance, role, non-done backlog task, oracle, proof, repository source, and local artifact checks |
| Path safety | Absolute, drive-relative, traversal, file URI, wrapped drive/file, malformed repo, missing file, and directory cases |
| Duplicate identities | Fixture, oracle, fact, proof, premise, and JSON-key rejection cases |
| Existing manifest truthfulness | Exact inventory; incomplete documents use explicit placeholder `/0`; invalid `/1` cannot be classified away |
| Production isolation | 13 token sentinels and live production scan with zero references |
| Bootstrap reachability | Root bootstrap invokes `tools/verify-r0-spec-001.ps1` and passes |

Every JSON document under `tests/r0-spec-001/valid/` and
`tests/r0-spec-001/invalid/` is enumerated by
`tests/r0-spec-001/cases.json`. The validator fails when an example is added
without a declared case.

## Existing manifest disposition

The following incomplete bootstrap manifests retain all original identity,
content, and task ownership while using an explicit non-contract version:

| Manifest | Version | Owning task |
| --- | --- | --- |
| `fixtures/ref-minimal-3dof/fixture-manifest.json` | `gnczmkn.fixture-manifest.placeholder/0` | `R0-SCI-002` |
| `fixtures/ref-yyz-001/fixture-manifest.json` | `gnczmkn.fixture-manifest.placeholder/0` | `R0-SCI-003` |
| `fixtures/ref-cavh-formula/fixture-manifest.json` | `gnczmkn.fixture-manifest.placeholder/0` | `R0-SCI-004` |
| `oracles/oracle-manifest.json` | `gnczmkn.oracle-manifest.placeholder/0` | `R0-LEG-002` |

`oracles/scientific-conventions/oracle-manifest.json` was structurally migrated
to conforming v1. Its four oracle identities, claims, tolerances, references,
and scientific meaning were preserved. The existing provenance reference and
four stable expected facts were made explicit. Its new SHA-256 was synchronized
to the provenance register and the R0-SCI acceptance evidence before the final
bootstrap run. Scientific Authority independently approved the migration after
a deterministic inverse transformation recovered the exact pre-migration bytes
and frozen digest.

## Decisions

- Stable schema identities use URNs and exact `/1` instance versions.
- REF, ORACLE, FACT, proof, premise, assertion, and generating-pass identity
  grammars are fixed in ADR-0009.
- Fixture/oracle status, oracle level, expected-fact comparison, proof class,
  proof result, and premise kind are closed registries.
- `fixture.authority` resolves to a repository role; `open_tasks` resolves only
  to a backlog task whose status is not `done`.
- Local source and artifact references resolve to repository files. `repo://`
  scheme matching is case-insensitive and the canonical written form is
  lowercase. Filesystem escapes and malformed reserved repo syntax fail closed.
- Expected-fact `field_refs` remain opaque R0 semantic identities. External
  non-file URIs remain opaque provenance locations.
- The standard-library validator implements only the schema keyword subset used
  by these contracts and rejects an unsupported keyword. The wrapper invokes
  Python with `-B`, so repeated validation does not create repository bytecode
  caches.
- Proof hash derivation, byte canonicalization, and a numeric tolerance
  container remain future decisions with real consumers.

## Review

- Architecture Lead: core contract approved after independent rerun of the full
  matrix and targeted repo-wrapper/path probes; no remaining core blocker.
- Validation Lead: approved the core contract and acceptance behavior after an
  independent full matrix, mutation checks, two frozen-hash audits, and
  bootstrap runs; no remaining technical blocker.
- Scientific Authority: approved the scientific oracle manifest migration
  after exact inverse reconstruction, hash synchronization, and independent
  science, provenance, schema, and bootstrap checks.
- Coordinator disposition: ADR-0009 accepted on 2026-08-09 after all required
  reviews and the cross-task scientific review closed without blockers.

## Frozen SHA-256 evidence

| File | SHA-256 |
| --- | --- |
| `docs/adr/0009-r0-bundle-and-plan-proof-schemas.md` | `0f0e3480e752a2ab341bf45070aae00a5f5c967f2280c9369d51941b6f7bac5e` |
| `specs/fixture-manifest.schema.json` | `2bf5b4f738c80d80432be8137a4c8e7b1087945ca0ecd07c10687078a79ff1cd` |
| `specs/oracle-manifest.schema.json` | `fb143065abe310ece70191bf54ae5d2ba99ee95520654ee2754cab5a822d7a80` |
| `specs/plan-proof-record.schema.json` | `561e035252b1f9c49bfba480d53f98f9913f30db10609a603db8c09601e93fea` |
| `tests/r0-spec-001/cases.json` | `bae1ef8db7018ba3c858e5e1c07446a5abd5757f2f701cecfbbf834568471566` |
| `tools/r0_spec_schema_validator.py` | `60ad5fc78443ed99f640ef9af860a71e2380ef681aa506283745c7226e653075` |
| `tools/verify-r0-spec-001.ps1` | `9d48e4b0683e4b3f9f8e8493c602e98518552d914b1fad0a7a7f53abd0fcf589` |
| `docs/quality/r0-spec-001/acceptance-and-failure-plan.md` | `688f12f9439f040de0a5f703e439e236b990e19ec856e24f1cf4116a1f437e67` |
| `docs/quality/r0-spec-001/existing-manifest-classification.json` | `200130e42b86a55adc91ee25418f17978b7ac8d872714e064707166b7a3738b8` |
| `oracles/scientific-conventions/oracle-manifest.json` | `36533390b59dac37657ae13f39759d1538e950457a4e383f48eafc62dd376a69` |
| `tools/bootstrap.ps1` | `4e0e22beb9c12bf11eb978acc5f810225005b374abbcc16d90472fa1f89d0848` |
| `docs/quality/r0-spec-001/scientific-authority-migration-review.md` | `a2105d3295edf0d0dd3a96cf52c5b53d03103d6593dff7d4fb119900775c9c55` |

## Residual risks and dependency changes

- Four placeholder manifests remain gate-blocked under their named R0 tasks.
- R0 tolerance policy remains explanatory text; a machine numeric tolerance
  container requires a later schema decision and consumer evidence.
- Proof suffix derivation and canonical bytes remain intentionally open; no hash
  algorithm was selected by this task.
- Bootstrap now depends on the task-local conformance command as a governance
  gate. No production target depends on any R0 schema or validator.
- No third-party dependency, thread model, runtime API, package ABI, or legacy
  production dependency was introduced.

Recommended next work is the coordinator's R0 gate audit. Scientific bundle
owners can later migrate each placeholder only after its real provenance,
expected facts, oracle references, and tolerance evidence close.
