# Legacy CSV semantic-boundary reference

`REF-LEGACY-CSV-001` captures the smallest frozen dataset that binds each recorded row to its `t_k` boundary while allowing the CSV encoding to change.

The fixture starts at altitude `1000 m` with vertical velocity `10 m/s`, applies constant acceleration `-2 m/s²`, and records `t0`, `t1=0.5 s`, and `t2=1 s`. The semantic rows are:

```text
t0: altitude=1000 m,    vertical_velocity=10 m/s
t1: altitude=1004.75 m, vertical_velocity=9 m/s
t2: altitude=1009 m,    vertical_velocity=8 m/s
```

The bundle keeps capture, semantic mapping, and independent calculation separate:

- [`legacy_capture.cpp`](legacy_capture.cpp) uses the frozen Simulator, RK4, AutoDataLogger, and CsvRecordSink public surfaces in the fixed Legacy extraction environment;
- [`legacy-run-1.csv`](legacy-run-1.csv) and [`legacy-run-2.csv`](legacy-run-2.csv) are repository-LF representations of byte-identical CRLF captures, with both identities pinned in [`input.json`](input.json);
- [`semantic-fields.json`](semantic-fields.json) maps the three required Legacy columns to fixture-local field identities with unit, frame, and value-boundary metadata;
- [`reference.json`](../../oracles/ref-legacy-csv/reference.json) contains the 50-digit Decimal trajectory, encoding equivalence rule, failure cases, tolerance boundary, and accepted disposition;
- the repository C++17 probe independently maps columns by header and evaluates the analytic semantic rows without including or linking Legacy.

Run both executable checks through CTest:

```powershell
ctest --preset dev -R "r0.legacy-csv" --output-on-failure
```

Column order, unmapped columns, numeric text formatting, session filename, and output directory do not participate in the semantic comparison. Positive equivalence checks re-encode required values with finite Decimal-equivalent exponent notation, replace every unmapped value with opaque text, and add a duplicate unmapped header. Every required semantic header must occur exactly once; direct failures remove or duplicate a required column and inject `NaN` or infinities into every required numeric field. The accepted disposition preserves each row's `t_k` boundary and published-state projection while retiring the Legacy encoding and path shape. The bundle is `executable`; `R0-SCI-003` still needs to freeze target YYZ field tolerances.
