# Navit routing correctness and performance testing

This directory contains two separate correctness references and an opt-in
production-C integration harness. **Normal builds retain the legacy routing implementation; the experimental arrival solver is opt-in and its validation remains incomplete.**

## Test layers

| Layer | Runner | What it validates |
| --- | --- | --- |
| Independent oracle | `python3 -m unittest discover -s tests/routing -p 'test_*.py' -v` | Dijkstra over directed movements, independent JSON road fixtures |
| Experimental arrival-state regression | `ctest --test-dir build-routing -R '^routing_production_search
| State-aware cost reference | `ctest --test-dir build-routing -R '^routing_turn_state_candidate$'` | Separate test-only Dijkstra against live Navit segments |
| Explicit movement paths | `ctest --test-dir build-routing -R '^routing_turn_state_path$'` | Candidate paths, arrival-dependent routing, traffic-cost revisions |
| Movement semantics | `ctest --test-dir build-routing -R '^routing_turn_state_movement$'` | Directions, road continuity, terminal costs, partial starts, traffic changes |
| Synthetic benchmark | `navit_routing_benchmark` | Correctness-gated full search versus incremental update measurements |

Configure the experimental regression with `cmake -S . -B build-routing -DNAVIT_ROUTING_TEST_HOOKS=ON -DNAVIT_ROUTING_ARRIVAL_EXPERIMENT=ON`, then build the relevant test targets.
`NAVIT_ROUTING_TEST_HOOKS=ON` alone exposes the production test harness without selecting the experimental algorithm.
The independent Python oracle has no C dependencies and uses Python 3.11
or newer. The `NAVIT_ROUTING_TURN_SHADOW=ON` option enables
non-authoritative, debug-only evaluation during `route_path_new()`:
it does **not** change which route Navit follows.

## Reproducible correctness findings

CORE-004 shows a through-traffic penalty depending on an incoming road.
INCR-006 shows a successor change without a cost-to-go change that
leaves upstream values stale. CORE-004B demonstrates that different
incoming movements at one junction require different continuations.
These are **verified failures of the current point-based routing**
using actual Navit production graph operations, not merely differences
between abstract algorithms.

The candidate solver computes costs and reconstructs directed paths
for those fixtures. Additional cases establish terminal-partial-road
penalties, terminal reversal and directional legality, plus
projected-start choices. These remain synthetic graph tests: realistic
map geometry, restrictions, waypoints and GPS movement require an
independent geographic integration suite.

## Benchmarking

The `Routing performance validation` workflow builds an optimized
synthetic benchmark, checks independent dynamic-programming oracle
costs for each operation, validates CSV completeness and uploads raw
samples and environment details. It includes no geographic or
waypoint route extraction and compares unlike update strategies:
legacy LPA* repairs while the candidate recomputes from scratch.
Do **not** infer production performance from these samples.

See `TURN_STATE_DESIGN.md` for the remaining state-aware integration
and incremental-repair requirements. See `PRODUCTION.md` for the
original production-harness procedure and historical setup notes.

The geographic suite `navit-gps/routing-qa` has not yet been
integrated into these workflows. No pull request is required for
testing on the dedicated feature branch.
` | Cases 4–6 pass only with `NAVIT_ROUTING_ARRIVAL_EXPERIMENT=ON`; legacy point-based cases remain Red |
| State-aware cost reference | `ctest --test-dir build-routing -R '^routing_turn_state_candidate$'` | Separate test-only Dijkstra against live Navit segments |
| Explicit movement paths | `ctest --test-dir build-routing -R '^routing_turn_state_path$'` | Candidate paths, arrival-dependent routing, traffic-cost revisions |
| Movement semantics | `ctest --test-dir build-routing -R '^routing_turn_state_movement$'` | Directions, road continuity, terminal costs, partial starts, traffic changes |
| Synthetic benchmark | `navit_routing_benchmark` | Correctness-gated full search versus incremental update measurements |

Configure first with `cmake -S . -B build-routing -DNAVIT_ROUTING_TEST_HOOKS=ON`, then build the relevant test targets.
The C harness is enabled with `-DNAVIT_ROUTING_TEST_HOOKS=ON`.
The independent Python oracle has no C dependencies and uses Python 3.11
or newer. The `NAVIT_ROUTING_TURN_SHADOW=ON` option enables
non-authoritative, debug-only evaluation during `route_path_new()`:
it does **not** change which route Navit follows.

## Reproducible correctness findings

CORE-004 shows a through-traffic penalty depending on an incoming road.
INCR-006 shows a successor change without a cost-to-go change that
leaves upstream values stale. CORE-004B demonstrates that different
incoming movements at one junction require different continuations.
These are **verified failures of the current point-based routing**
using actual Navit production graph operations, not merely differences
between abstract algorithms.

The candidate solver computes costs and reconstructs directed paths
for those fixtures. Additional cases establish terminal-partial-road
penalties, terminal reversal and directional legality, plus
projected-start choices. These remain synthetic graph tests: realistic
map geometry, restrictions, waypoints and GPS movement require an
independent geographic integration suite.

## Benchmarking

The `Routing performance validation` workflow builds an optimized
synthetic benchmark, checks independent dynamic-programming oracle
costs for each operation, validates CSV completeness and uploads raw
samples and environment details. It includes no geographic or
waypoint route extraction and compares unlike update strategies:
legacy LPA* repairs while the candidate recomputes from scratch.
Do **not** infer production performance from these samples.

See `TURN_STATE_DESIGN.md` for the remaining state-aware integration
and incremental-repair requirements. See `PRODUCTION.md` for the
original production-harness procedure and historical setup notes.

The geographic suite `navit-gps/routing-qa` has not yet been
integrated into these workflows. No pull request is required for
testing on the dedicated feature branch.
