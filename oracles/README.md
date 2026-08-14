# Scientific and behavior oracles

Oracles provide independent facts used to accept, reject or explain an implementation result. Legacy output can seed evidence capture; final scientific authority comes from analytic results, paper data, independent implementation, convergence or approved engineering reference.

`oracle-manifest.json` starts with the seven preserved runtime claims listed by the target blueprint. Planned entries already declare expected facts and tolerance policy; they remain `planned` until R0 produces executable inputs, expected values and artifact hashes.

`ref-minimal-3dof/reference.json` is the executable high-precision analytic oracle for `REF-MINIMAL-3DOF-001`. Its comparator addresses semantic case and state fields directly and remains independent of Legacy output layout.

`ref-legacy-sync-commit/reference.json` is the first executable Legacy behavior slice. It compares a 50-digit Decimal derivation with an isolated C++17 event journal, verifies the frozen archive and passing Legacy test evidence, and rejects an early-commit mutation. Its disposition stays pending until the repository owner accepts the recommended Preserve/Retire split.
