# R0-ARCH-001 verification summary

## Run identity

- Task: `R0-ARCH-001`
- Run date: 2026-08-09
- Local time: 2026-08-09T16:35:26+08:00
- Platform: Windows 11
- CMake: 4.0.3
- Generator: Ninja 1.13.0
- C++ compiler: MSYS2 GCC 15.1.0
- Preset: `dev`

## Task-specific verification

Command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/verify-r0-arch-001.ps1 -SelfTest
```

Result: passed.

- Canonical terms: 309, comprising 300 glossary rows and 9 ADR-0011 scientific overlay rows.
- Retired aliases: 20.
- Complete shared enums: 13.
- Shared key compositions: 4.
- AuthorityDomains: 4.
- Modules and logical boundaries: 11.
- Registered legacy mappings: 23; audit-only mappings: 4; target responsibilities: 33.
- Negative self-tests: 10/10 passed.

The negative suite proves duplicate identities, non-scalar authority, authoritative-source drift, scientific overlay drift, unknown/duplicate/cyclic dependencies, forbidden Kernel-to-Compiler reachability, CMake graph drift, missing legacy coverage, incomplete ownership, and unresolved target terminology are rejected.

## Repository gate verification

Command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/bootstrap.ps1
```

Result: passed.

- Configure and generation: passed.
- Build: passed; no pending Ninja work.
- CTest: 5/5 passed.
- Repository verification: passed; 34 JSON files, 65 task entries, and 85 Markdown files validated in this run.
- Provenance policy: positive records passed and every declared negative case was rejected, including lowercase `noassertion` for external clearance.

## Scope and review state

- No production source, runtime contract, package implementation, Compiler behavior, Session behavior, plugin runtime, language binding, frontend, or legacy source was changed by this work package.
- ADR-0010 is Accepted after Product Owner review.
- ADR-0011 is Accepted after Scientific Authority and Product Owner review.
- Task status remains controlled by the coordinator through `docs/tasks/backlog.json`.
