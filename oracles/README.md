# Scientific and behavior oracles

Oracles provide independent facts used to accept, reject or explain an implementation result. Legacy output can seed evidence capture; final scientific authority comes from analytic results, paper data, independent implementation, convergence or approved engineering reference.

`oracle-manifest.json` starts with the seven preserved runtime claims listed by the target blueprint. Planned entries already declare expected facts and tolerance policy; they remain `planned` until R0 produces executable inputs, expected values and artifact hashes.

`ref-minimal-3dof/reference.json` is the executable high-precision analytic oracle for `REF-MINIMAL-3DOF-001`. Its comparator addresses semantic case and state fields directly and remains independent of Legacy output layout.

`ref-legacy-sync-commit/reference.json` is an executable Legacy behavior slice. It compares a 50-digit Decimal derivation with an isolated C++17 event journal, verifies the frozen archive and passing Legacy test evidence, and rejects an early-commit mutation. Its accepted disposition preserves the candidate barrier and committed-`t_k` cross-system reads while retiring Legacy implementation structure.

`ref-legacy-publish/reference.json` is an executable Legacy behavior slice. It checks a 50-digit constant-acceleration boundary trajectory against an isolated C++17 publish projection, verifies deterministic reruns and frozen provenance, and rejects a publish-time committed-state mutation. Its accepted disposition preserves read-only publication and `t_k` truth refresh while retiring the Legacy surface.

`ref-legacy-phase/reference.json` is an executable Legacy behavior slice. It verifies two raw traces from an external harness compiled against the frozen archive, compares them with an independent C++17 scheduler probe, and rejects phase swaps and duplicate phases. Its accepted disposition preserves fixed macro-phase order while retiring priority values, registration/config tie-breaks and the Legacy callback surface.

`ref-legacy-continuous-group/reference.json` is a capturing Legacy behavior slice. It compares two raw four-stage Legacy RK traces with 50-digit Decimal and independent C++17 joint-state references, distinguishes the final position `9` from the split snapshot result `10`, and rejects invalid or duplicate scope membership. Its IntegrationScopePlan Preserve recommendation and Legacy group-surface Retire recommendation remain pending.

`ref-legacy-csv/reference.json` is a capturing Legacy behavior slice. It compares two byte-identical Legacy CSV captures with 50-digit Decimal and an independent C++17 semantic projection, maps required fields by header identity, accepts column permutation, and rejects missing or temporally inconsistent rows. Its `t_k`/published-boundary Preserve recommendation and Legacy encoding Retire recommendation remain pending; target YYZ field tolerances remain pending R0-SCI-003.

`ref-legacy-stop/reference.json` is a capturing Legacy behavior slice. It verifies that a frozen t0 evaluator reads the flushed stopping-state row before returning true, compares two captures with an independent Python/Decimal reference and C++17 timeline probe, and rejects termination-before-record, a missing terminal row and post-stop advancement. Legacy reason text is excluded from semantic comparison; target `Observation` and `RunOutcome` artifacts remain pending.
