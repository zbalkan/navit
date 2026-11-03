# Routing correctness test foundation

This directory contains independent deterministic routing reference fixtures. It does not yet test Navit's production routing code.

## Status

**Oracle tests only.** All-green results validate the fixture model and independent solver, **not** the correctness of Navit's `route.c`. A future test-only adapter must execute the actual production routing functions against equivalent fixtures. Only then can G-03/INCR-006 be called Red or Green with respect to Navit.

The oracle uses Dijkstra over directed movement states `(junction, incoming road)`, including explicit turn restrictions and incoming-road-dependent penalties. All transition costs are nonnegative integers.

The `through_traffic.json` fixture demonstrates a mathematical successor-dependent penalty counterexample. It is not proof that Navit's actual profile flags reproduce it. Mapping to Navit's real road profiles, graph clones, turn rules, destination candidates and cost units is a separate correctness requirement.

## Run

From the repository root:

`python3 -m unittest discover -s tests/routing -p 'test_*.py' -v`

Python 3.11 or newer; standard library only. The new GitHub workflow runs independently of the existing Navit build workflow.

## Fixture coverage

| Fixture | Specification | Scope |
| --- | --- | --- |
| `directed.json` | CORE-001 | Directed traversal |
| `alternative.json` | CORE-002 | Alternative route cost |
| `forbidden_turn.json` | CORE-003 | Prohibited turn |
| `through_traffic.json` | CORE-004 | Successor-dependent penalty |
| `equal_successor.json` | INCR-006 (static prerequisite) | Equal-cost continuations |
| `traffic_events.json` | INCR-002/003/004 (oracle only) | Increase, decrease, closure, reopening |

## Red–Green–Refactor sequencing

1. Verify independent reference results and fixtures (this commit).
2. Implement test-only production integration, preserving `route.c` behavior.
3. Compare production routes against the independent oracle and record baseline characterization separately. Mark tests Red only when an actual production run disagrees with an independently justified expectation.
4. Make the smallest production change necessary for one observed Red test.
5. Refactor with all correctness and characterization tests still passing.

No direct `value`/`rhs` invariant checks, map-backed D-Bus tests, or performance claims exist in this commit.

The independent `navit-gps/routing-qa` repository remains the established map-backed geographic regression suite; integration with its fixtures and modernization of its older Python 2 runner should be separate subsequent work.
