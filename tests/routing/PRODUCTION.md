# Test-only production routing harness

This harness compiles the **actual** `navit/route.c` code into `navit_core` and exposes private functions only when `NAVIT_ROUTING_TEST_HOOKS=ON`.

## Coverage

The C test constructs a four-vertex, four-road graph using Navit's graph insertion and Fibonacci heap. It exercises:

* Initial destination-oriented propagation (expected shortest route cost 5).
* Cost increase on the selected road and incremental LPA* repair (expected cost 8).
* Cost decrease and incremental LPA* repair (expected cost 3).

Road lengths and a 36 km/h profile yield numeric tenths-of-seconds costs in the production cost function. The tests inspect `value`, `rhs`, selected successor, and termination queue membership.

These are deterministic **production code smoke tests**, not a complete routing oracle. They do not exercise map loading, actual off-road destination matching, traffic-message ingestion, restriction cloning, or a real vehicle position. The Python reference oracle remains independent.

## Build locally

`cmake -S . -B build-routing -DNAVIT_ROUTING_TEST_HOOKS=ON -DSVG2PNG=OFF -DXPM2PNG=OFF`

`cmake --build build-routing --target navit_routing_production_test --parallel 2`

`ctest --test-dir build-routing -R '^routing_production_search$' --output-on-failure`

The option defaults to OFF. Ordinary builds contain neither these test hook symbols nor the test executable.

## Verification status

The files are committed for CI execution. **Full C compilation and execution have not been verified locally**, because the current analysis environment lacks Navit's GLib development headers and the repository cannot be cloned over its restricted network. A GitHub Actions result is required before declaring this integration Green.

## Next test cycle

Once CI confirms basic production integration, add an exact test translation for the through-traffic fixture and the equal-cost successor-change fixture. Do not claim those correctness risks are reproducible until the actual production implementation has been exercised.
