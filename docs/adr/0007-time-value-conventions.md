# ADR-0007: Scientific time value conventions

- Status: Accepted
- Date: 2026-08-09
- Owners: Scientific Authority
- Reviewers: Architecture Lead
- Review evidence: R0-SCI-001 read-only Architecture Lead review, 2026-08-09
- Related tasks: R0-SCI-001
- Architecture references: 03 §9, §22

## Context

Simulation progression, data sampling, validity, publication, and host-clock audit describe different facts. Passing all of them as an unlabeled `double time` permits invalid arithmetic and hides clock-domain mismatches. A minimal semantic baseline is required before runtime lifecycle or artifact schemas are implemented.

This ADR fixes value semantics and scientific serialization invariants. It does not choose scheduler behavior, step boundaries, event localization, storage precision, or a final public schema.

## Decision

The following rules apply.

1. The five distinct time kinds are `SimulationTime`, `Duration`, `SampleTime`, `ValidTime`, and `WallTime`. Public scientific boundaries retain the kind; no generic unlabeled time scalar is permitted.
2. `SimulationTime` is logical time in a declared simulation clock domain. Its origin/epoch is run metadata. It has no implicit relationship to host wall time.
3. `Duration` is a signed interval expressed in SI seconds. A caller such as an integrator or timeout policy may impose positivity as its own precondition.
4. `SampleTime` records when data was sampled or computed. `ValidTime` records when data applies. `publish_time` records the simulation time at which a value entered published cycle state. These fields remain separate when their numeric values happen to match.
5. A finite validity interval is half-open, `[valid_from, valid_until)`. Equal endpoints form an empty interval. A reversed interval is invalid. An unbounded endpoint uses explicit absence; infinity is not a serialized endpoint.
6. Scientific simulation, sample, valid, and duration values serialize with a finite numeric value, canonical unit id `s`, semantic kind, and source clock domain where applicable. The exact container layout and codec remain with the contract/schema work package.
7. `WallTime` remains isolated from scientific time. Audit instants require an explicit civil/UTC representation and clock source in the future artifact schema; elapsed performance and timeout measurements use `Duration` from a declared monotonic clock.
8. Valid arithmetic is typed. Examples include `SimulationTime + Duration -> SimulationTime` and subtraction of two `SimulationTime` values in the same clock domain yielding `Duration`. Mixed clock domains fail unless an explicit, versioned clock conversion is supplied.
9. Time equality does not establish data order. A separate monotonic `sequence` detects duplicates, gaps, and ordering when multiple records share a time.
10. NaN and infinity are invalid for every scientific time value.

## Consequences

- Positive: sample, validity, publish, and execution time remain distinguishable in fixtures and future artifacts.
- Costs: contracts carry a time kind and clock identity, and adapters must perform explicit clock conversions.
- Risks: concrete storage precision and UTC/leap-second handling remain open until a real artifact/checkpoint consumer is reviewed.
- Modules kept unchanged: scheduler, Session, CycleFrame, Artifact schemas, adapters, and legacy reference.

## Alternatives considered

- One floating-point timestamp for every use: rejected because it permits wall/simulation mixing and loses validity semantics.
- Serialize unbounded validity with infinity: rejected because JSON and cross-tool handling of non-finite values is inconsistent.
- Make every duration non-negative: rejected because signed differences are mathematically useful; positivity belongs to the consuming operation.

## Verification

- `tests/scientific_conventions_properties.cpp` uses distinct test-local types and checks permitted arithmetic, same-domain comparison, half-open validity, and failure paths.
- `oracles/scientific-conventions/reference.py` independently checks interval and clock-domain behavior.
- `docs/quality/r0-sci-001-acceptance.md` lists reversed intervals, mixed clocks, and non-finite time as mandatory failures.

## Supersession rule

Reopen this ADR when a checkpoint, external ephemeris, UTC audit, or event-location consumer demonstrates a required precision or time-scale rule that this semantic baseline cannot represent. A superseding ADR must include clock conversion, leap handling, serialization, and migration evidence.
