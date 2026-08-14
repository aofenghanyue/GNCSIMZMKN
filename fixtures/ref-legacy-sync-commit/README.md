# Legacy synchronized-commit reference

`REF-LEGACY-SYNC-COMMIT-001` isolates the smallest Legacy case that distinguishes a candidate barrier from an early cross-system commit.

The frozen test starts with `mass=10` and `position=0`, evaluates `mass'=-2` and `position'=committed mass`, and advances one `dt=1` RK4 step. Candidate evaluation against one committed snapshot yields `mass=8` and `position=10`. Committing mass before evaluating position yields the discriminating control `position=8`.

The bundle keeps three evidence layers separate:

- [`input.json`](input.json) pins the raw fixture bytes, Legacy archive, relevant source entries and recorded runtime test;
- [`reference.json`](../../oracles/ref-legacy-sync-commit/reference.json) contains the Decimal result, event partial order, failure case and pending disposition recommendation;
- the C++17 probe emits an independent journal and deliberately reverses commit iteration, demonstrating that commit container order is outside the preserved fact.

Run the two executable checks through CTest:

```powershell
ctest --preset dev -R "r0.legacy-sync-commit" --output-on-failure
```

The bundle remains `capturing` until the repository owner accepts the recommended Preserve/Retire split. It does not include, link or execute Legacy production code.
