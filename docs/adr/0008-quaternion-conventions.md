# ADR-0008: Quaternion direction, multiplication, and serialization conventions

- Status: Accepted
- Date: 2026-08-09
- Owners: Scientific Authority
- Reviewers: Architecture Lead
- Review evidence: R0-SCI-001 read-only Architecture Lead review, 2026-08-09
- Related tasks: R0-SCI-001
- Architecture references: 03 §8, §22

## Context

Quaternion coefficient order, active/passive meaning, multiplication convention, and composition order vary between tools. A numerically valid unit quaternion can produce the inverse physical result when any one of these choices is implicit. The convention must therefore be fixed as one connected decision and verified independently.

This ADR fixes semantic mathematics and coefficient interchange. It does not introduce a production quaternion class or an external-tool adapter.

## Decision

The following rules apply.

1. A quaternion is written `q = (w, x, y, z)` with scalar coefficient first.
2. Multiplication is the Hamilton product. For `p = (pw, pv)` and `q = (qw, qv)`:

   ```text
   p * q = (pw*qw - dot(pv,qv),
            pw*qv + qw*pv + cross(pv,qv))
   ```

3. `q_to_from` represents the same passive coordinate transformation as `R_to_from` in ADR-0006. With a vector embedded as the pure quaternion `(0, v_from)`:

   ```text
   (0, v_to) = inverse(q_to_from) * (0, v_from) * q_to_from
   ```

4. Quaternion composition follows the frame path and the passive convention:

   ```text
   q_c_a = q_b_a * q_c_b
   ```

   This is paired with `R_c_a = R_c_b * R_b_a`.
5. The serialized coefficient sequence is exactly `[w, x, y, z]`. Frame direction, time, normalization status, and quality are surrounding attitude metadata and cannot be inferred from the four coefficients.
6. Deserialization requires exactly four finite coefficients. A zero-norm quaternion is a domain failure.
7. Rotation use requires unit norm within policy tolerance. `Reject` fails on a non-unit value. `NormalizeWithFlag` may normalize a finite nonzero value and must record that action. Silent normalization is not permitted.
8. `q` and `-q` represent the same rotation and compare equal under a rotation-aware tolerance. Raw coefficient equality is not a semantic attitude comparison. A unique sign rule for hashing remains a later codec/hash decision.
9. Euler interchange always declares axis sequence, intrinsic or extrinsic interpretation, angle unit, and singular range. No default Euler convention exists.
10. The canonical fixtures use right-handed axes and column vectors. For a desired passive matrix rotation of `+theta` about unit axis `u`, the matching Hamilton quaternion is `(cos(theta/2), -u*sin(theta/2))` under this ADR.

## Consequences

- Positive: matrix, quaternion, and tool-adapter results have one reviewable direction and composition rule.
- Costs: libraries with active or vector-first conventions require explicit adapter mappings.
- Risks: coefficient-level hashing remains open and must account for the `q`/`-q` equivalence.
- Modules kept unchanged: foundation production headers, domain attitude contracts, Compiler, Kernel, adapters, and legacy reference.

## Alternatives considered

- Use the active transform `q * v * inverse(q)`: rejected because it would conflict with the target `q_to_from` passive coordinate convention.
- Serialize `[x, y, z, w]`: rejected because the target architecture has already selected scalar-first order.
- Use JPL multiplication: rejected because mixed Hamilton/JPL composition is a common inverse-result failure and the target architecture selects Hamilton multiplication.
- Canonicalize sign during R0 serialization: deferred to the future hash/codec ADR so that 180-degree and signed-zero behavior can be specified with byte-level evidence.

## Verification

- `tests/scientific_conventions_properties.cpp` checks single-axis rotations, inverse, composition, matrix equivalence, coefficient order, `q`/`-q`, normalization policy, and malformed inputs.
- `oracles/scientific-conventions/reference.py` is an independent standard-library implementation.
- `oracles/scientific-conventions/cross_tool_check.py` compares fixed single-axis and composed cases and records maximum component, norm, determinant, and round-trip errors.

## Supersession rule

Reopen this ADR only with a demonstrated external scientific authority or target consumer that cannot be mapped losslessly by an adapter. A superseding ADR must update the frame convention, all quaternion fixtures, serialization evidence, and downstream schemas in one reviewed change.
