# ADR-0006: Frame identity and transform direction conventions

- Status: Accepted
- Date: 2026-08-09
- Owners: Scientific Authority
- Reviewers: Architecture Lead
- Review evidence: R0-SCI-001 read-only Architecture Lead review, 2026-08-09
- Related tasks: R0-SCI-001
- Architecture references: 03 §7, §8.1, §22

## Context

A bare three-vector cannot identify whether it is a point, free vector, velocity, force, or moment. Transform names such as `rotation` also leave source and target direction open to interpretation. Frame identity, matrix direction, composition, and affine behavior must be fixed before domain ports are designed.

This ADR fixes the mathematical convention. It does not create a coordinate graph, transform provider, runtime cache, or public C++ contract.

## Decision

The following rules apply.

1. Mathematical vectors are column vectors.
2. Canonical physical frames used by attitude rotations are right-handed. A left-handed external convention must be converted at its adapter and must record that mapping.
3. A frame definition carries a namespace-qualified, versioned semantic identity. Examples include `frame.inertial.j2000@1`, `frame.earth.ecef@1`, and `frame.vehicle.body@1`. Instance ownership is separate identity metadata. Exact parser and object schema remain with the contract/schema work package.
4. Every frame definition declares axis directions, handedness, origin meaning, owner when applicable, time model, and semantic version. A short display label has no identity authority.
5. `R_to_from` maps coordinates of the same geometric free vector from `from` into `to`:

   ```text
   v_to = R_to_from * v_from
   ```

6. `R_to_from` is a proper orthogonal matrix within declared tolerance: `R^T R = I` and `det(R) = +1`. Its inverse is `R_from_to = R_to_from^T`.
7. Composition follows the written path:

   ```text
   R_c_a = R_c_b * R_b_a
   ```

8. A free-vector transform applies rotation only. An affine point transform uses:

   ```text
   p_to = R_to_from * p_from + t_to_from
   ```

   where `t_to_from` is the position of the `from` origin expressed in `to`. Point and free-vector operations are distinct.
9. Velocity, acceleration, wrench, and covariance transforms require their own semantic operations. Their additional kinematic or Jacobian terms cannot be obtained by reusing a point-transform operation.
10. A time-varying transform query carries the requested simulation/valid time and returns its sample/valid time and quality. A timeless matrix cannot silently stand in for time-varying frame data.

## Consequences

- Positive: direction is visible in every transform identity and compositions can be reviewed from names alone.
- Costs: adapters and domain contracts must provide frame metadata and distinct point/vector operations.
- Risks: exact instance-frame identifier syntax remains open and must be frozen with its first schema consumer.
- Modules kept unchanged: coordinate graph, Compiler, Kernel, model packages, adapters, and legacy reference.

## Alternatives considered

- Use active rotations as the primary public convention: rejected because the architecture blueprint already defines coordinate transformations between frames.
- Use row vectors: rejected because it reverses multiplication order from the target mathematical baseline.
- Permit reflection matrices in attitude values: rejected because a unit quaternion represents only proper rotations; handedness conversion belongs at a declared boundary.

## Verification

- `tests/scientific_conventions_properties.cpp` checks orthogonality, determinant, inverse, composition, and point/free-vector separation over fixed and deterministic generated rotations.
- `oracles/scientific-conventions/reference.py` implements the same frame equations independently.
- `oracles/scientific-conventions/cross_tool_check.py` compares fixed cases from Python and C++ and retains per-case errors.

## Supersession rule

Reopen this ADR only with a physical frame convention that cannot be represented by a declared adapter mapping or evidence that the direction/composition rule produces an incorrect coordinate transformation. A superseding ADR must update frame, quaternion, and cross-tool evidence together.
