# R0-ARCH-001 Scientific Authority review

- Task: `R0-ARCH-001`
- Reviewer: `codex-r0-science` (`scientific_authority`)
- Review date: 2026-08-09
- Decision: approved; no blocking findings
- Review mode: read-only

## Evidence reviewed

- ADR-0010 and ADR-0011 record their required independent dispositions and are
  `Accepted`.
- The dependency map declares `consumer_to_dependency`, matches the root CMake
  direct links, is acyclic, and preserves the three declared forbidden
  reachability constraints.
- Each term, enum, key, AuthorityDomain, module, and each of 33 legacy target
  responsibilities resolves to one scalar registry authority and one declared
  owner where ownership applies.
- All 23 registered legacy names are covered; four audit-only concepts remain
  explicitly distinguished; every target term resolves to registered non-Legacy
  vocabulary.
- The nine ADR-0011 overlay terms, eleven ASCII and case-sensitive SI unit ids,
  five scientific time kinds, `R_to_from`, Hamilton passive `q_to_from`, and
  `[w, x, y, z]` agree with ADR-0005 through ADR-0008.
- No production source, application, package, adapter, user path, or CMake target
  consumes the three R0 governance documents.

## Independent execution

`tools/verify-r0-arch-001.ps1 -SelfTest` passed with 309 canonical terms, 20
retired aliases, 11 module boundaries, 23 registered legacy mappings, and 10/10
negative self-tests. The reviewer also matched every source snapshot hash and
all eight artifact hashes recorded by the conformance report, then independently
confirmed the authority, owner, and overlay counts.

The review accepts these files only as R0 governance metadata. Production types,
payload schemas, codecs, frame graphs, clock services, and R1-R8 runtime behavior
remain outside this task.
