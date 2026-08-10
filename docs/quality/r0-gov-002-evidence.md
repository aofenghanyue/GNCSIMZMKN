# R0-GOV-002 verification evidence

## Scope

- Task: `R0-GOV-002`
- Baseline date: 2026-08-09
- Baseline commit: `7b05c29f5863d6230fad9285371dc98bd2a30c0d`
- Inputs: repository root, package/fixture/oracle manifests, `reference/legacy/source-manifest.json`, and artifact lineage rules

## Recorded policy gap

The pre-task scan found only `LICENSE-STATUS.md`. It declares that no distribution license has been selected. The repository had no `LICENSE`, `COPYING`, `NOTICE`, third-party inventory, or reusable provenance checklist.

The legacy source manifest records commit and archive integrity, while its redistribution rights remain unreviewed. The initial YYZ fixture asks for provenance confirmation and does not yet provide an itemized license decision.

These gaps block external publication, redistribution, third-party source/data import, and any claim that generated evidence can be shared outside the internal research group.

## Acceptance design

The governance package must define explicit rules for:

1. repository source code;
2. first-party and third-party assets/data;
3. dependencies and external tools;
4. the frozen legacy archive;
5. generated artifacts and evidence bundles;
6. internal use, external sharing, publication review, and prohibited actions.

The provenance checklist must fail review when identity, origin, license or permission basis, integrity hash, scope, restrictions, reviewer, or downstream propagation is missing.

## Implemented controls

- `ADR-0004` proposes an internal-only default and an external-sharing fail-closed gate.
- `docs/handoff/license-and-provenance-policy.md` covers source, dependencies, assets/data/formulas, tools, the legacy archive, generated artifacts, and evidence bundles.
- `docs/quality/provenance-register.json` records the first-party bootstrap snapshot and legacy archive with rights status, allowed/prohibited uses, integrity, classification, propagation, and review.
- The governance validator uses a closed subject-type vocabulary and an explicit scientific-context flag.
- Scientific records require domain, units, frames, time convention, target identity, reference/oracle refs, independent-reference confirmation, and independence basis.
- Restricted and unreviewed material must propagate `block-external-sharing` with inheritance enabled.
- Generated output must resolve upstream records and lineage, preserve all upstream requirements, and use a classification at least as restrictive as every input.

## Executable conformance cases

The policy suite currently proves:

- the live register and a complete scientific record pass;
- missing rights are rejected;
- missing scientific context is rejected;
- a reused implementation posing as an independent reference is rejected;
- disabled restriction inheritance is rejected;
- missing generated-output upstream refs are rejected;
- generated-output classification weaker than its input is rejected;
- an unknown subject type cannot bypass scientific review.

## Review history

Scientific Authority review round 1 found three blockers: open subject typing, no explicit independent-reference assertion, and incomplete restriction propagation. The implementation was revised to close all three findings and the expanded negative suite passed. `codex-r0-science` approved round 2 on 2026-08-09 after independently rerunning the conformance suite. The reviewer notes that scientific field content still requires human authority review, which the policy gate preserves.

Architecture Lead review round 1 found that `NOASSERTION` could pass the cleared/external branch. The validator now rejects case-normalized `NOASSERTION` and `NONE`, and a lowercase approved-external negative record proves the failure path. `codex-r0-architecture` approved round 2 on 2026-08-09 after independently running the policy and repository checks.

## Final verification

- ADR-0004 status: `Accepted`.
- Required reviews: Architecture Lead and Scientific Authority approved after their blocking findings were resolved.
- Live register: 4 records passed, covering the first-party bootstrap snapshot, read-only legacy archive, `ORACLE-R0-SCI-CONVENTIONS-001` scientific fixture, and generated `R0-LEG-001` reproduction evidence.
- Conformance: 2 positive records passed; 7 invalid fixture files exercised missing rights, missing scientific context, false independence, disabled inheritance, missing generated upstream refs, weaker generated classification, unknown subject type, and `NOASSERTION` external-clearance rejection.
- `tools/bootstrap.ps1`: passed on 2026-08-09.
- CTest: 5/5 passed.
- Repository guard: passed with 34 JSON files, 65 task entries, and 82 Markdown files at the recorded run.
- Policy subprocess exit-code propagation: passed with caller-visible exit code 0.

| Artifact | SHA-256 |
| --- | --- |
| `LICENSE-STATUS.md` | `61441b7fd58959be7fbc8c416da7e10613b2c471ce411db7ce37f9792575b19f` |
| `docs/adr/0004-internal-license-and-provenance-policy.md` | `aa4fdbbe6dcdce88f3d6c01f3c6054f777ac262e19270ccc3958a9a1c6216f47` |
| `docs/handoff/license-and-provenance-policy.md` | `f150b77ffd476ba21707e2e927ef0c9b1026b99bf6f97d046c8766d789d89539` |
| `docs/quality/provenance-register.json` | `cd79f0d04a3b2c0fdca339548f402f8ffcbd29c2be214d451f1df8bc48c9ef38` |
| `tools/verify-provenance-record.ps1` | `3d887452db2ef3a60317640959a5409244fedbe1c08efb010c4cd528a4262df6` |
| `tools/test-provenance-policy.ps1` | `e39d232f7ac24fbd78d74dfa78079f953eccd285fa087b31fb246880aa8d556e` |

## Decisions and handoff

- The repository remains internal-only and supplies no external distribution grant.
- Unknown, ambiguous, missing, `NOASSERTION`, and `NONE` rights states cannot authorize external sharing.
- Scientific provenance content remains subject to Scientific Authority judgment even when structural validation passes.
- Generated evidence inherits the most restrictive upstream classification and requirements.
- Future public release still requires rights-holder confirmation, a distribution-license ADR, notices, per-item clearance, and legal review for unresolved issues.
- No backlog dependency was added. Completion satisfies the `R0-GOV-002` edge for `R0-GATE-001`; all other R0 gate dependencies remain active.
- Recommended next unclaimed first-wave task: `R0-SPEC-001` for Architecture Lead + Validation Lead.
