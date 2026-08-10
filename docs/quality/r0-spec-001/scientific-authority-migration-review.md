# R0-SCI-001 oracle manifest v1 migration review

- Review role: Scientific Authority
- Reviewer: `codex-r0-science`
- Review date: 2026-08-09
- Reviewed artifact: `oracles/scientific-conventions/oracle-manifest.json`
- Current SHA-256: `36533390b59dac37657ae13f39759d1538e950457a4e383f48eafc62dd376a69`
- Disposition: approved
- Blocking findings: none

## Semantic preservation

A deterministic read-only inverse transformation removed only the new top-level
`provenance_refs` block and the four single-element `expected_facts` blocks.
The result exactly reconstructed the 3199-byte pre-migration manifest with
SHA-256 `eedc2ddb9451697b201c6eda817e791d0618f2dfca4d4a908528300463c59edd`,
which matches the original R0-SCI-001 frozen digest.

The original `schema_version`, `oracle_set_id`, status, oracle collection, and
every oracle `id`, level, status, claim, reference kind, tolerance policy,
source reference, and artifact reference therefore remain byte-for-byte and
order-for-order unchanged. The new provenance reference resolves to
`prov.oracle.r0-scientific-conventions@1`. Its subject and target identity are
`ORACLE-R0-SCI-CONVENTIONS-001`, and its scientific context retains the finite
proper-rotation, declared SI-conversion, typed-time, frame, unit, independent
Python-reference, and independence-basis constraints.

Each new expected-fact statement equals its source claim. SI and time facts use
`predicate`; frame and quaternion facts use `numeric`. These comparison classes
agree with the original exact-failure and numeric-tolerance semantics. All four
FACT identities are unique and conform to ADR-0009.

## Hash synchronization

The current digest matches all three independently checked locations:

- the manifest bytes;
- `prov.oracle.r0-scientific-conventions@1.integrity.value` in the provenance register;
- the frozen artifact table in `docs/quality/r0-sci-001-acceptance.md`.

All provenance component hashes also match the current case set, Python
reference, cross-tool checker, C++ property source, and retained report. The
migration changes only the manifest evidence envelope.

## Independent verification

- Scientific CTest label: 3/3 passed.
- C++ property executable: 559 checks passed.
- Python standard-library reference: 554 checks passed.
- R0-SPEC conformance: 6 valid cases passed, 52 required invalid cases and one
  unsupported-schema case rejected, one v1 manifest conformed, four placeholder
  manifests remained gate-blocked, and zero failures occurred.
- Provenance direct validation: four records passed.
- Provenance policy suite: all positive and declared negative cases passed.
- Full `tools/bootstrap.ps1`: exit 0; CTest 5/5, repository verification,
  provenance, and R0-SPEC gates passed.
- Oracle and tool trees contained no `__pycache__` directories.

## Scope and residual conditions

The v1 manifest, schemas, and validator remain R0 evidence and governance
artifacts. The production scan reported zero forbidden references. No public
production type, runtime schema, Compiler, Session, plugin runtime, Python API,
frontend, compatibility layer, or third-party dependency was introduced.
External sharing remains blocked by ADR-0004. Numeric tolerance containers,
canonical bytes and hashes, and production quantity, quaternion, and time
contracts remain future decisions.
