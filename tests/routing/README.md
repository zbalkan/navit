# Navit routing correctness and performance testing

The routing tests distinguish legacy production behavior from the opt-in arrival-state experiment. The Python oracle is independent of Navit's C routing implementation. Normal builds retain legacy routing; the experimental solver requires `NAVIT_ROUTING_ARRIVAL_EXPERIMENT=ON`.

## Test layers

| Layer | Runner | What it validates |
| --- | --- | --- |
| Independent oracle | `python3 -m unittest discover -s tests/routing -p 'test_*.py' -v` | Dijkstra over directed movements and independent JSON road fixtures |
| Experimental arrival-state regression | `ctest --test-dir build-routing -R '^routing_production_search$'` | Cases 4–6 pass when `NAVIT_ROUTING_ARRIVAL_EXPERIMENT=ON`; legacy point-based cases remain verified Red |
| Legacy routing smoke | `build-routing-legacy/navit/navit_routing_production_test --legacy-smoke` | Cases 1–3 with the experimental solver disabled |
| State-aware cost reference | `ctest --test-dir build-routing -R '^routing_turn_state_candidate$'` | Independent movement-state search over Navit segments |
| Explicit movement paths | `ctest --test-dir build-routing -R '^routing_turn_state_path$'` | Candidate paths, arrival-dependent routing and traffic-cost revisions |
| Movement semantics | `ctest --test-dir build-routing -R '^routing_turn_state_movement$'` | Directions, road continuity, terminal costs, partial starts and traffic changes |
| Synthetic benchmark | `navit_routing_benchmark` | Separately labeled legacy and arrival-experiment builds against the movement-state reference |

Configure and build the experimental regression with `cmake -S . -B build-routing -DNAVIT_ROUTING_TEST_HOOKS=ON -DNAVIT_ROUTING_ARRIVAL_EXPERIMENT=ON`. Configure the legacy smoke separately with `cmake -S . -B build-routing-legacy -DNAVIT_ROUTING_TEST_HOOKS=ON -DNAVIT_ROUTING_ARRIVAL_EXPERIMENT=OFF`. Include your platform's normal Navit CMake dependencies and graphics-disable options as needed. Build `navit_routing_production_test` in each directory before invoking the listed commands. The hook option alone exposes the test harness; it does not select the experimental algorithm.

The independent Python oracle uses Python 3.11 or newer and does not require compiling Navit. `NAVIT_ROUTING_TURN_SHADOW=ON` enables non-authoritative diagnostics during `route_path_new()` without selecting its route.

## Reproducible correctness findings

CORE-004 demonstrates an incoming-road-dependent through-traffic penalty. INCR-006 demonstrates a successor change without a scalar cost-to-go change that leaves upstream values stale. CORE-004B demonstrates different optimal continuations from different arrival contexts at one junction. These are verified failures of legacy point-based routing using Navit graph operations.

The experimental solver passes the synthetic correction fixtures, terminal-partial-road penalties, terminal reversal and direction legality, and projected-start choices. Those results do not establish equivalence with a full incoming-movement router or validate geographic map topology, waypoints and GPS movement.

## Benchmarking

The `Routing performance validation` workflow runs two distinct builds and artifacts. The `legacy` job disables the arrival experiment and uses a zero through-traffic penalty to measure the legacy LPA*-like search against the movement-state reference. The `arrival-experiment` job explicitly enables the new solver and uses the default 9,000 penalty. Each job checks costs against an independent, mode-matched dynamic-programming oracle and uploads its own CSV and environment metadata. Results from the two modes are **not directly comparable as performance measurements**, because the routing penalties differ.

Both jobs measure a synthetic directed chain or shortcut DAG, not real map geometry or waypoint navigation. The reference movement-state solver recomputes from scratch. Do not infer production performance from the synthetic samples.

See `BENCHMARK.md` for the schema and `TURN_STATE_DESIGN.md` for the design limitations. See `PRODUCTION.md` for the original Red fixtures and historical harness procedure. The geographic suite `navit-gps/routing-qa` has not been integrated into these workflows.
