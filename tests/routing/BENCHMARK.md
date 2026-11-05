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


## Reproducible measurements and reports

Run `Routing performance validation` for a push to `bench/routing-*` or manually from GitHub Actions. The workflow runs **two independent jobs** with their own CMake build trees, correctness validation and artifacts:

| Mode | Compile flag | Through-traffic penalty | Point-search CSV label |
| --- | --- | ---: | --- |
| Legacy baseline | experiment OFF | 0 | `legacy` |
| Restricted-arrival experiment | experiment ON | 9000 | `arrival-experiment` |

Both jobs compare their point search with the same full-recomputation movement-state reference under their respective cost models. These jobs are **not** a cross-mode speedup experiment. The legacy job is a historical zero-penalty control, not a claim that unmodified Navit at penalty 9000 is routing-correct.

Each job uploads `routing-benchmark.csv`, `routing-benchmark-environment.txt`, `routing-benchmark-summary.json` and `routing-benchmark-summary.md`. The Markdown summary is also published to the GitHub Actions step summary. Preserve raw measurements for independent review; a summary cannot substitute for the underlying CSV.

The reporting script requires successful independent cost validation and exactly 25 complete, paired repetitions for each size/topology/operation.

Every update stratum is labeled **changed** or **unchanged** according to whether the independent oracle's optimal cost changes after the road update. An unchanged optimal cost does not prove that the physical path is identical. It does establish that the update can be cheaper than a true cost-changing repair. For instance, the restricted-arrival shortcut scenario at 600 and 2400 vertices contains cost-unchanged updates, and its dramatic update ratios must not be generalized to rerouting. Chain scenarios provide cost-changing updates under both modes. These classes must be considered separately when comparing latency. It reports median and nearest-rank p95 timings in milliseconds separately for each algorithm. The paired movement-to-point timing ratio is the median of **per-repetition** ratios; values above 1 indicate the point search was faster in that stratum. A zero-duration denominator makes the ratio undefined, rather than manufacturing a speedup.

The measurement schedule alternates algorithm order between repetitions and performs three unreported warm-ups per workload. Benchmark timings exclude graph construction, resetting the graph, oracle evaluation, and CSV serialization. For updates the point search includes local invalidation and incremental repair; the movement search recomputes the path from scratch. This is a workload-method comparison, not identical algorithmic work.

Local reporting of an already captured artifact uses:

`python3 tests/routing/validate_benchmark_csv.py routing-benchmark.csv legacy`

`python3 tests/routing/summarize_benchmark.py routing-benchmark.csv --mode legacy --json-out summary.json --markdown-out summary.md`

Replace `legacy` with `arrival-experiment` for the experimental artifact. The summary generator also runs the CSV validator and refuses missing rows, mismatched implementation labels, inaccurate oracle costs, malformed records and duplicate repetitions. The reporting tests run with the independent oracle workflow.

### Interpretation and remaining acceptance work

The summary quantifies **synthetic CI-runner behavior** only. The 25 samples share a process, machine and runner environment; they are not 25 statistically independent deployments. No significance level, confidence interval or hard latency acceptance threshold is inferred from them.

Before using the results to approve a production routing change, obtain map-backed navigation traces and representative incremental road-closure/reopen, traffic-change and reroute workloads. Include graph memory and heap extraction counts, validate path continuity and ETA, run on repeatable dedicated hardware, and explicitly approve performance budgets. In particular, the current two-arrival-class solver does not model arbitrary incoming-segment turn histories.
