# Routing performance validation

This CI benchmark is a synthetic, correctness-gated comparison, not a production navigation result. Both solvers use the same actual Navit graph, with 50, 200, 600, 1200 or 2400 vertices, either one-way chains or two-hop shortcuts.

The workflow runs separately labeled modes. `legacy` compiles with the arrival-state experiment disabled and uses through-traffic penalty 0. `arrival-experiment` explicitly enables the experimental arrival-state solver with penalty 9,000. Because these modes have different cost models, their wall times are not directly comparable.

The point-based solver in each mode is measured as initial backward propagation or incremental updates. The movement-state candidate performs full Dijkstra search for **all** operations; it does not incrementally repair routes. Graph construction, reset, oracle evaluation and CSV output are excluded from timings. For legacy incremental changes, timing includes local LPA* update preparation and shortest-path propagation; for the candidate it includes full recomputation. Both represent the processing cost of the already-applied road change.

Each topology/size combination runs three unreported warm-ups, then 25 repetitions. Every repetition measures full computation, road-cost increase (10 to 30), and subsequent decrease (30 to 10). The algorithm measurement order alternates across repetitions.

An independent dynamic-programming oracle calculates route costs on the known forward-only DAG. Both measured costs must match it; disagreements are recorded and cause benchmark failure. Even failures during warm-up generate diagnostic CSV rows, with negative repetition values. Failed correctness samples cannot be treated as successful latency measurements.

## Limitations

- GitHub-hosted runner timings are CI regression measurements, not reproducible hardware benchmarks. The workflow records commit, runner, CPU and compiler metadata in a separate artifact file.
- Traffic closures/reopenings, moving vehicle starts, route deviations, projected positions, and partial-road terminal costs require additional fixtures and oracles.
- Timing uses CLOCK_MONOTONIC where available, with a GLib microsecond-resolution fallback. The elapsed_ns unit does not guarantee nanosecond accuracy.
- The existing production LPA* Red cases 4–6 remain independently failing correctness cases.
- The movement-state candidate is not selected by production Navit routing.

## CSV schema

scenario,vertices,edges,repeat,implementation,operation,elapsed_ns,cost,oracle_cost,correctness

Operations are full, increase, and decrease. Implementation names are `legacy` and `movement` in the baseline artifact, or `arrival-experiment` and `movement` in the experiment artifact. Each successful artifact produces 1500 measurement rows (2 topologies × 5 sizes × 25 repetitions × 3 operations × 2 implementations).
Each benchmark mode builds in Release mode and uploads a separate mode-labeled CSV artifact and environment file even when the benchmark step fails. A build failure before benchmark execution cannot produce a CSV.
