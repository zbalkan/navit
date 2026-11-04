# Test-only production routing harness

This harness compiles the **actual** `navit/route.c` code into `navit_core` and exposes private functions only when `NAVIT_ROUTING_TEST_HOOKS=ON`.

## Coverage

The production harness constructs a four-vertex/four-road fixture for the first three cases and separate five-vertex/five-road fixtures for successor-dependent routing. It exercises:

* Initial destination-oriented propagation (expected shortest route cost 5).
* Cost increase on the selected road and incremental LPA* repair (expected cost 8).
* Cost decrease and incremental LPA* repair (expected cost 3).
* CORE-004: incoming-road-dependent through-traffic cost (turn-aware expected cost 4).
* INCR-006: paired traffic-cost updates that change B's selected continuation without changing B's cost-to-go (turn-aware expected cost 4).

Road lengths and a 36 km/h profile yield numeric tenths-of-seconds costs in the production cost function. The tests inspect `value`, `rhs`, selected successor, and termination queue membership.

These are deterministic **production code tests**, not a complete routing oracle. CORE-004 and INCR-006 are correctness specifications expected to challenge the point-state recurrence: they must be marked Red only when the compiled production code actually disagrees with their independently computed turn-aware costs. They also assert fixture preconditions (specific chosen successors). A failed precondition is a fixture problem, not confirmation of a Navit bug. They do not exercise map loading, actual off-road destination matching, traffic-message ingestion, restriction cloning, or a real vehicle position. The Python reference oracle remains independent.

## Build locally

`cmake -S . -B build-routing -DNAVIT_ROUTING_TEST_HOOKS=ON -DSVG2PNG=OFF -DXPM2PNG=OFF`

`cmake --build build-routing --target navit_routing_production_test --parallel 2`

`ctest --test-dir build-routing -R '^routing_production_search$' --output-on-failure`

The option defaults to OFF. Ordinary builds contain neither these test hook symbols nor the test executable.

## Verification status

The earlier production smoke tests (cases 1–3) compiled and passed in GitHub Actions on commit `d46da77dc72a4cb85c4f5e75a4134fa78a5c7bd2`. The two new cases require their own GitHub Actions results before classifying them as Red or Green. An expected Red case will fail the production regression CI job during this TDD phase. Neither that failure nor a standalone oracle success justifies a production fix without independently checking the test conditions.

## Next test cycle

Confirm the CORE-004 and INCR-006 test preconditions and observed costs from CI logs. If either reveals a genuine discrepancy, preserve its failing fixture unchanged and begin a separate minimal production-code Green fix. Keep the independent oracle and existing regression tests as acceptance gates.
