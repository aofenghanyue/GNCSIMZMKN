# ADR-0011: R0 scientific terminology overlay

- Status: Accepted
- Date: 2026-08-09
- Owners: Architecture Lead
- Required reviewers: Scientific Authority, Product Owner
- Reviewed by: Scientific Authority (approved 2026-08-09); Product Owner (approved 2026-08-09)
- Related tasks: R0-ARCH-001, R0-SCI-001
- Architecture references: reference-glossary sections 1 and 3, 03 sections 6 through 9 and 22, ADR-0005 through ADR-0008, ADR-0010

## Context

The cross-book glossary requires every shared term to be registered before it is used elsewhere. The scientific architecture booklet already uses `SimulationTime`, `Duration`, `SampleTime`, `ValidTime`, `WallTime`, `UnitId`, and `FrameId`. R0-SCI-001 has now frozen their scientific meaning together with the directional notations `R_to_from` and `q_to_from`, the canonical SI unit identifiers, and quaternion coefficient order. None of these names currently has a canonical row in `reference-glossary.md`.

The design-notes tree is a read-only architecture input for R0-ARCH-001. Silently omitting these names would make the machine baseline incomplete. Editing the architecture source inside this work package would erase the evidence of the cross-book registry conflict.

## Decision

Create a narrow R0 terminology overlay inside `specs/architecture/r0/terminology-baseline.json`. The overlay registers exactly these additional canonical terms:

- `UnitId` and `FrameId`;
- `SimulationTime`, `Duration`, `SampleTime`, `ValidTime`, and `WallTime`;
- `R_to_from` and `q_to_from`.

All supplemental terms remain `V1` target vocabulary. Each term has the single `terminology-registry` authority reference and one detailed source in ADR-0005, ADR-0006, ADR-0007, or ADR-0008. ADR-0011 becomes the single registry authority path for the combined R0 baseline and records which rows were imported from the glossary and which rows close this scientific gap.

The same overlay adds two closed R0 value registries and one key composition:

1. `CanonicalSIUnitId` has the ASCII, case-sensitive values `m`, `s`, `kg`, `rad`, `m/s`, `m/s^2`, `rad/s`, `N`, `N*m`, `Pa`, and `K` from ADR-0005.
2. `ScientificTimeKind` has the values `SimulationTime`, `Duration`, `SampleTime`, `ValidTime`, and `WallTime` from ADR-0007.
3. `QuaternionCoefficientOrder` has the ordered components `w`, `x`, `y`, and `z` from ADR-0008.

This overlay freezes terminology and interchange facts only. It does not create a public C++ type, JSON container, codec, hash rule, frame graph, clock service, quantity library, quaternion class, or runtime dependency. Future contract tasks must cite these names and still own their schemas and production representations.

## Consequences

- Positive: the R0 machine registry covers the scientific vocabulary already shared by architecture and accepted scientific review.
- Positive: each supplemental term, enum, and key retains one authority and one owner role.
- Costs: a later architecture-source refresh must merge these rows into the canonical glossary and retire the overlay without changing identity.
- Risks: accepting a different spelling in the source glossary requires an explicit superseding decision and migration evidence.
- Modules kept unchanged: foundation, contracts, model_sdk, compiler, kernel, evidence, workflow, application, adapters, packages, user code, and legacy reference.

## Alternatives considered

- Omit the scientific terms until a later source refresh: rejected because the R0 terminology baseline would already be incomplete.
- Modify `reference-glossary.md` in this task: rejected because design-notes are task inputs and the conflict must remain reviewable.
- Give every scientific ADR its own terminology registry: rejected because the acceptance rule requires one registry authority for shared names.
- Define production types now: rejected because R1-R8 remain locked and the scientific ADRs explicitly leave container and type design to later contract tasks.

## Verification

- The R0 architecture validator finds each supplemental term exactly once.
- `CanonicalSIUnitId`, `ScientificTimeKind`, and `QuaternionCoefficientOrder` each have one scalar authority and one declared owner role.
- Unit identifiers are ASCII and match ADR-0005 exactly.
- Time kind values match ADR-0007 exactly.
- Quaternion component order matches ADR-0008 exactly.
- No production source, CMake target, or runtime schema consumes the overlay.

## Supersession rule

Supersede this overlay after the canonical architecture glossary contains equivalent rows with the same spelling and meaning, or when a reviewed scientific authority requires a different convention. Supersession must update the machine baseline, affected ADRs, executable evidence, and migration notes together.
