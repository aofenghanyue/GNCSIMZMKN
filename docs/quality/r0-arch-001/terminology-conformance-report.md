# R0-ARCH-001 terminology conformance report

- Status: passed
- Generated at UTC: 2026-08-09T08:35:42.1998507Z
- Governing decisions: ADR-0010 (Accepted) and ADR-0011 (Accepted)
- Scope: R0 governance metadata only

## Baseline counts

| Item | Count |
| --- | ---: |
| Canonical terms | 309 |
| Retired aliases | 20 |
| Complete shared enums | 13 |
| Shared key compositions | 4 |
| AuthorityDomains | 4 |
| Modules and logical boundaries | 11 |
| Registered legacy mappings | 23 |
| Audit-only legacy mappings | 4 |
| Target responsibilities | 33 |

## Conformance result

The committed baselines exactly match the authoritative source snapshot. Canonical terms and aliases are unique. Every enum, key, AuthorityDomain, module, and legacy target responsibility resolves to one registry authority and one accountable role. The dependency graph is acyclic, matches the root CMake direct links, and satisfies the forbidden reachability declared by ADR-0003. Every registered Legacy term has a target ownership mapping, and every target term resolves to a non-Legacy canonical term.

## Negative evidence

| Failure ID | Result | Detected issues |
| --- | --- | ---: |
| ARCH-TERM-001 | passed | 1 |
| ARCH-TERM-002 | passed | 2 |
| ARCH-TERM-003 | passed | 1 |
| ARCH-TERM-004 | passed | 5 |
| ARCH-DAG-001 | passed | 5 |
| ARCH-DAG-002 | passed | 1 |
| ARCH-DAG-003 | passed | 1 |
| ARCH-LEG-002 | passed | 1 |
| ARCH-LEG-001 | passed | 1 |
| ARCH-LEG-003 | passed | 1 |

## Source snapshots

| Source | SHA-256 |
| --- | --- |
| design-notes/gnczmkn-architecture-roadmap/reference-glossary.md | 69b119abfb933c05a4c603385f80c6a6ccee49e3916a6f29cda0e0d1b956f572 |
| design-notes/gnczmkn-architecture-roadmap/02-layered-reference-architecture.md | a5c6610d52edca947912368a122937c1f43fdc3347f94a5380e6dc574568f4d3 |
| docs/adr/0010-r0-architecture-baseline-format.md | 5a239f0e57d68b22f42931987e818837236bad2baba7cbf76f5e30c473a9bc1c |
| docs/adr/0011-r0-scientific-terminology-overlay.md | 38bca6f241d75378a19ee2e78ccfce58d9bc335429685adedab9c4b1c1e4fd3b |
| docs/adr/0005-si-unit-and-numeric-conventions.md | a9e42f8ff0f15d1b6bb778dfffabcc66d7718366212077f4cd0d6c5cd89c5a56 |
| docs/adr/0006-frame-transform-conventions.md | 55b1e6785f556ff02c4fdfebf46b29b0fe3a49c18aaec4eacce541e989bf111c |
| docs/adr/0007-time-value-conventions.md | eb3749371d334b9f0dfdf2c029ba2ff5acee82acbb64ed88268a6de84d5da24d |
| docs/adr/0008-quaternion-conventions.md | a6f9e2fac58a3c4b3df6e2f0e99bec3ed0a94b383a1925bb76190023c3e498ea |
| docs/adr/0003-initial-module-dependency-dag.md | 52b6197eb01645b79edafc5eb9984590fe59e8523c6e309e99631d1241f83542 |
| project-manifest.json | eb8a1145d95c92396073a4a9ece7d5d41f84b2e286c0842331c1d9e8653ca5ad |
| CMakeLists.txt | 182aaf9b34daab3436bd195f422d896c30acdd57e83cc64fe84d48c708e57465 |
| design-notes/gnczmkn-architecture-roadmap/01-current-architecture-deep-audit.md | 8fe4c76b353a5fb5699da5348d6340cb6a75fe63766b189a9c1e136efdd3a763 |

## Artifact hashes

| Artifact | SHA-256 |
| --- | --- |
| specs/architecture/r0/terminology-baseline.json | 3aef1736adb3a4d9599786341dbf9fef08ed9ab9312e84a4169c6d3301ebf5c6 |
| specs/architecture/r0/module-dependency-map.json | 949040aaed0ca27aa29164571c4049916c3378530d13d13989348bf45be6ec0a |
| specs/architecture/r0/legacy-to-target-ownership-map.json | f834462ca5e9ea0cacaf9e0f0cdf090d307458678b7bc1f4323c3908dd7deeb0 |
| tools/verify-r0-arch-001.ps1 | 2f7bd4f30d0dc7f3df9293cdacbbcc4afd6374e6515758e792bc1c4f044c5179 |
| docs/adr/0010-r0-architecture-baseline-format.md | 5a239f0e57d68b22f42931987e818837236bad2baba7cbf76f5e30c473a9bc1c |
| docs/adr/0011-r0-scientific-terminology-overlay.md | 38bca6f241d75378a19ee2e78ccfce58d9bc335429685adedab9c4b1c1e4fd3b |
| docs/quality/r0-arch-001/acceptance-and-failure-plan.md | 8879fa43998dab84ceb2f32809bbdbf37971daf264492a7c2b385638635df3aa |
| docs/quality/r0-arch-001/verification-summary.md | 8c828578ac7885e9d988208c6ad37418ff47cb5dd7021d8948efad6eac05684f |

## Limits and review status

- ADR-0010 status: Accepted; Product Owner approval is recorded in the decision.
- ADR-0011 status: Accepted; Scientific Authority and Product Owner approvals are recorded in the decision.
- These files are R0 governance metadata. Production runtime consumption is outside this task.
- Detailed semantic authority remains with the architecture sources listed by each term.
