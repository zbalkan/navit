# Routing performance validation

This CI benchmark is a synthetic, correctness-gated comparison, not a production navigation result. Both solvers use the same actual Navit graph, with 50, 200 or 600 vertices, either one-way chains or two-hop shortcuts.

Legacy LPA* is measured as initial backward propagation or incremental updates. The movement-state candidate performs full Dijkstra search for **all** operations; it does not incrementally repair routes. Graph construction, reset, oracle evaluation and CSV output are excluded from timings.

Each topology/size combination runs three unreported warm-ups, then 25 repetitions. Every repetition measures full computation, road-cost increase (10 to 30), and subsequent decrease (30 to 10). The algorithm measurement order alternates across repetitions.

An independent dynamic-programming oracle calculates route costs on the known forward-only DAG. Both measured costs must match it; disagreements are recorded and cause benchmark failure. Failed correctness samples cannot be treated as successful latency measurements.

## Limitations

- GitHub-hosted runner timings are CI regression measurements, not reproducible hardware benchmarks.
- Traffic closures/reopenings, moving vehicle starts, route deviations, projected positions, and partial-road terminal costs require additional fixtures and oracles.
- The monotonic timer has microsecond resolution. Nanosecond CSV values are multiples of 1,000, not nanosecond-precision observations.
- The existing production LPA* Red cases 4–6 remain independently failing correctness cases.
- The movement-state candidate is not selected by production Navit routing.

## CSV schema

scenario,vertices,edges,repeat,implementation,operation,elapsed_ns,cost,oracle_cost,correctness

Operations are full, increase, and decrease. Implementations are legacy and movement. A full successful run produces 900 measurement rows (2 topologies × 3 sizes × 25 repetitions × 3 operations × 2 implementations).