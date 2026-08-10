# R0-GOV-001 verification evidence

## Scope

- Task: `R0-GOV-001`
- Input: `docs/team/role-assignments.json`
- Guard: `tools/verify-repository.ps1`
- Baseline date: 2026-08-09
- Baseline environment: Windows, PowerShell, CMake 4.0.3, GCC 15.1.0, Ninja generator

## Acceptance design

The repository guard checks every required role for:

1. a non-empty assignee;
2. a non-empty reviewer;
3. a reviewer identity distinct from the assignee.

The accepted toolchain decision must identify the language standard, minimum CMake version, generator policy, supported CI platforms, and deferred ABI or deployment commitments.

## Recorded failure path

Command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/verify-repository.ps1
```

Result before role assignment: failed as designed with 16 issues. Each of the eight required roles reported a missing assignee and a missing reviewer.

A second negative run temporarily assigned `product_owner` to review its own work. The guard failed as designed with one issue:

```text
Required team role product_owner must use a reviewer distinct from its assignee.
```

## Final verification

- Baseline commit: `7b05c29f5863d6230fad9285371dc98bd2a30c0d`
- Role assignment result: all eight required roles have a distinct assignee and reviewer; the optional Application Lead is also assigned.
- Toolchain decision: ADR-0002 records the local reference lane, two required CI lanes, version floor, generator policy, and deferred compatibility commitments.
- `release` configure/build: passed with GCC 15.1.0, CMake 4.0.3, and Ninja 1.13.0.
- `ctest --preset release`: 2/2 tests passed.
- `tools/verify-repository.ps1`: passed; 14 JSON files, 65 task entries, and 67 Markdown files checked.

| Artifact | SHA-256 |
| --- | --- |
| `docs/team/role-assignments.json` | `8624fece1181931f24dd77435946cd40bd6a94f784828b6dfb2fe8ecc6ecde98` |
| `docs/adr/0002-cpp17-cmake-modular-monolith.md` | `8edea760e2f162368821552d3fbc32d3722082c50381c5520eea8463b13a72de` |
| `tools/verify-repository.ps1` | `6c52edcec5175238b918d32b4a35f89f64288f950e85b52e33abe144ca3176f8` |

## Review

- Product Owner: `codex-r0-coordinator` — prepared and verified on 2026-08-09.
- Architecture Lead: `codex-r0-architecture` — approved on 2026-08-09 with no blocking findings. The reviewer independently reran the repository guard and matched all three recorded artifact hashes.

## Decisions and handoff

- Required roles use named R0 execution seats and independent reviewers; the scientific and architecture authorities cross-review high-risk decisions.
- Toolchain support is defined by one recorded local reference lane plus mandatory Windows and Ubuntu CI lanes.
- Moving hosted-runner images require the actual compiler identity to remain visible in configure evidence.
- Stable ABI, dynamic package loading, multi-process deployment, and cross-toolchain binary compatibility remain deferred.
- No task dependency was added. Completion satisfies the `R0-GOV-001` edge for downstream tasks; their remaining dependencies and stage gates still apply.
- Recommended next task for the Product Owner seat: `R0-GOV-002`.
