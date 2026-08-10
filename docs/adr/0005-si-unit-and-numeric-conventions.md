# ADR-0005: SI unit and numeric conventions

- Status: Accepted
- Date: 2026-08-09
- Owners: Scientific Authority
- Reviewers: Architecture Lead
- Review evidence: R0-SCI-001 read-only Architecture Lead review, 2026-08-09
- Related tasks: R0-SCI-001
- Architecture references: 03 §5.2, §6, §9, §22

## Context

Scientific values need one computational unit convention before domain contracts and schemas can be reviewed. Unit inference from field names, implicit degree/radian conversion, non-finite payloads, and silent fallback for unknown units would make cross-tool evidence ambiguous. The authoritative external metrology reference is the [BIPM SI Brochure, 9th edition, current revision](https://www.bipm.org/en/si-brochure-9).

This ADR fixes scientific invariants and machine unit identifiers. It does not introduce a production quantity library or a final public JSON container.

## Decision

The following rules apply.

1. IEEE-754 binary64 is the first authoritative computational scalar. Scientific values must be finite at validated boundaries. A later scalar backend must preserve the same domain and serialization semantics.
2. Runtime physical values use coherent SI units. Plane angle is carried explicitly in radians even though the radian is dimensionless in SI analysis.
3. The first canonical machine unit identifiers are ASCII and case-sensitive:

   | Quantity | Canonical unit id |
   | --- | --- |
   | length and position | `m` |
   | duration and scientific time coordinate | `s` |
   | mass | `kg` |
   | plane angle | `rad` |
   | speed | `m/s` |
   | acceleration | `m/s^2` |
   | angular speed | `rad/s` |
   | force | `N` |
   | moment | `N*m` |
   | pressure | `Pa` |
   | thermodynamic temperature | `K` |

4. Human display symbols may use typography such as `m/s²` or `N·m`. Machine contracts continue to use the identifiers above.
5. Non-SI values are converted only at a declared adapter or domain-converter boundary. The effective input records the original value/unit and the canonical value/unit.
6. The R0 executable reference fixes these boundary conversions: `km -> m` uses factor `1000`; `deg -> rad` uses factor `pi/180`; `degC -> K` uses offset `273.15`. A result below `0 K` is a domain failure.
7. An unknown or dimensionally incompatible unit id is an explicit failure. No identity fallback, field-name inference, or automatic clamp is allowed.
8. Scientific serialization carries a finite numeric value and an explicit canonical unit id. The enclosing object layout, schema version, codec, and hash remain owned by the contract/schema work package.

## Consequences

- Positive: independent tools share one unit baseline and can compare numeric values directly after declared normalization.
- Costs: adapters must retain source-unit provenance and implement checked conversions.
- Risks: unit aliases and compound-unit grammar remain deliberately small; additions require a reviewed registry change.
- Modules kept unchanged: all production modules, Compiler, Kernel, packages, adapters, and legacy reference.

## Alternatives considered

- Carry source units through runtime computations: rejected because every algorithm would acquire conversion branches and comparison evidence would become context-dependent.
- Apply a compile-time dimensional-analysis library to all mathematics: deferred until a real consumer and dependency ADR exist.
- Accept free-form unit text: rejected because spelling aliases and ambiguous compound expressions cannot support stable machine evidence.

## Verification

- `tests/scientific_conventions_properties.cpp` checks binary64 assumptions, canonical identifiers, conversions, and rejection of unknown/non-finite/domain-invalid inputs.
- `oracles/scientific-conventions/reference.py` independently checks the same scientific facts with the Python standard library.
- `docs/quality/r0-sci-001-acceptance.md` records the fixed success and failure paths.

## Supersession rule

Reopen this ADR only with a concrete scientific consumer that the listed SI convention cannot express, an updated authoritative metrology requirement, or cross-tool evidence showing the fixed identifiers or conversions are unsafe. A superseding ADR must include migration and round-trip evidence.
