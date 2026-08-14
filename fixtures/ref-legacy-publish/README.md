# Legacy publish-boundary reference

`REF-LEGACY-PUBLISH-001` isolates the smallest Legacy case that distinguishes a read-only publish projection from a publish operation that advances or mutates committed state.

The case starts at `t0=0` with altitude `1000 m`, vertical velocity `10 m/s` and constant acceleration `-2 m/s²`. Publishing at `t0` leaves committed state unchanged and exposes truth at sample time `0`. One `dt=0.5 s` advance commits altitude `1004.75 m` and velocity `9 m/s`; the old truth remains at sample time `0` until publishing at `t1=0.5 s` refreshes it from that committed boundary.

The bundle keeps three evidence layers separate:

- [`input.json`](input.json) pins the raw fixture bytes, Legacy archive, relevant source entries and recorded runtime test;
- [`reference.json`](../../oracles/ref-legacy-publish/reference.json) contains the 50-digit Decimal derivation, boundary timeline, mutation failure case and accepted disposition;
- the C++17 probe independently runs RK4, projects both boundary states, checks exact state identity and rejects a publish-time altitude mutation.

Run the two executable checks through CTest:

```powershell
ctest --preset dev -R "r0.legacy-publish" --output-on-failure
```

The bundle is `executable`. The accepted disposition preserves read-only publication and `t_k` truth refresh while retiring the Legacy task, context, callback and truth-storage surface. It does not include, link or execute Legacy production code.
