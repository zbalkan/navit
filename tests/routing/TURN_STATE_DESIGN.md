# Turn-aware routing: Green-phase design gate

Status: **design and test extension, not a production fix**.
Base: `test/routing-tdd-foundation` at `37e78cc6795b4c671adadf91eb2b809f44eb38b9`.

## Why a local LPA* relaxation fix is insufficient

The existing recurrence stores one `value`, `rhs`, and `seg` per
`route_graph_point`. However, `route_value_seg(profile, successor, edge, dir)`
uses `successor->seg` to apply an immediate-reversal constraint and a
through-traffic penalty. Thus the cost of entering the successor depends on
the chosen continuation.

CORE-004 demonstrates that a predecessor approaching B along a
through-traffic-limited edge needs the restricted continuation B->D.
CORE-004B (dual-ingress) also approaches B from a distinct, unrestricted
edge and needs B->C instead. Both positions must be routable **at the
same time**. Setting `B->seg` to either direction globally cannot satisfy
both requests.

INCR-006 additionally demonstrates that `B->seg` can change while
`B->value` stays constant, leaving a predecessor's `rhs` numerically
consistent but based on the wrong transition.

Do **not** make the Red cases pass by special-casing a graph-point's
predecessor, adjusting the expected oracle values to match Navit, or
forcing all entries at B to share a compromise successor.

## Required state identity

Use a directed movement state:

`S = (point, incoming_segment, incoming_direction)`

Incoming direction distinguishes the two ways to traverse a bidirectional
segment. States at the same coordinates, even with the same outgoing
options, are distinct when their incoming movement changes a subsequent
penalty or restriction.

A movement transition from `S=(v,e_in)` onto `e_out` is valid only when
the corresponding Navit direction flags, graph clones, and road
restrictions allow it. The transition cost must be a function of the
transition and immutable metric snapshot:

`C(S,e_out) = segment_travel_cost(e_out) + turn_penalty(e_in,e_out)`

For the existing through-traffic rule, the surcharge is paid at the
boundary from a restricted incoming road to an unrestricted outgoing
road; a terminal state without a continuing road must retain Navit's
correct destination semantics.

The exact conversion from existing `route_value_seg()`, `dst_val`,
`dst_seg`, and `route_path_new()` needs an explicit table before
replacing the search. Do not assume a generic graph edge corresponds
one-to-one to a legal Navit movement.

## Required search and result contracts

1. A state has its own `g` and `rhs` (or its own reference Dijkstra
   distance), parent/next movement, and queue membership.
2. Distinct incoming states at B may have distinct costs and successors.
3. The reference behavior remains exact, nonnegative shortest-path cost;
   ties may choose either optimal route.
4. Destination candidates and partial-road terminal costs remain
   part of the same cost function.
5. Traffic changes invalidate all movement transitions whose effective
   cost changed, including states affected by distorted segments.
6. Full and incremental search over the **same state graph** agree after
   a complete update. Changing the selected successor at B must not
   leave a stale predecessor.
7. Route extraction must follow the state-specific next movements.
   It must not silently fall back to the global `point->seg`.
8. Existing `route_graph_point` instances with identical coordinates
   but different restriction ancestry must not be merged by coordinate.
9. New code must preserve Navit's direction, size/weight restrictions,
   traffic distortions, planned turnaround handling, and waypoints.

## Complexity analysis

If the original directed graph has `M` legal traversals, there are
at most `M` incoming movement states plus start/terminal states.
Transitions may number `sum_v in_degree(v) * out_degree(v)`. We should
generate them on demand rather than blindly storing all pairs.
A binary-heap reference Dijkstra has typical
`O((|S|+|T|) log |S|)` work, where `T` are permitted movement
transitions. Incremental repair will have workload-dependent costs
and must not assume each edge-cost event is local in its consequences.

## Acceptance tests before production wiring

* CORE-001/002/003: basic directed routing and restriction behavior.
* CORE-004: restricted incoming A->B prefers B->D (cost 4).
* CORE-004B: with unrestricted X->B, X prefers B->C (cost 3)
  simultaneously with A preferring B->D (cost 4).
* INCR-006: updating two edges changes B's cheapest continuation but
  must not silently retain stale cost at A.
* Full-versus-incremental result agreement for every graph revision.
* Long predecessor chains and symmetric/oneway restrictions.
* Existing geographic routing-QA regression cases.

## Implemented stage-A candidate (not production routing)

A test-only movement-state Dijkstra prototype now operates on
`route_graph_segment` objects in
`tests/routing/production_state_solver.inc`. It calls the existing
production `route_value_seg()` for road traversal costs and adds
through-traffic and U-turn transition costs using the incoming
movement identity.

The isolated `navit_routing_state_test` target checks CORE-004,
INCR-006, and CORE-004B through cases 7, 8, and 9. It passed CTest
in GitHub Actions run 41810323519 (commit `12cdce2376348c58ed6e9f3ba092e065f415bac8`).
The independent Python reference is still a separate implementation.

The prototype only returns minimum cost for a junction destination.
It does **not** emit a movement-specific route, handle Navit's
terminal partial-road candidates or multi-waypoint extraction, or
update the production `value`/`rhs` structure. It does not
replace the existing LPA* engine. It uses deliberately simple
linear scans with superlinear/cubic worst-case work and is **not**
suitable for production use without a separate implementation review.

Production Red cases 4, 5, and 6 remain expected failures, regardless
of the isolated candidate passing. The production integration phase
must explicitly handle start-state selection, path extraction, graph
clones, metric revisions, and incremental invalidation.

## Implemented Stage B1: Explicit path selection (test-only)

`navit/route_turn_state.inc` implements a movement-aware graph
shortest-path search using Navit's actual segment costs, directional
checks, and Fibonacci heap. It records the predecessor **state**, then
reconstructs an ordered series of segment pointers and travel
directions. It supports a junction start with no previous road, or an
already-arrived state `(junction, incoming segment, direction)`.
The incoming segment is not charged or emitted twice.

The isolated tests `routing_turn_state_candidate` and
`routing_turn_state_path` check the costs and movement-specific paths
for CORE-004, INCR-006, and CORE-004B. The arrival-context test also
checks two distinct paths starting from the same junction with
different preceding segments.

**This is not a production Green result.** The new helper is compiled
only with `NAVIT_ROUTING_TEST_HOOKS`. Navit's normal path extraction
still uses `route_path_new()` and `point->seg`. The helper currently
uses GLib array types unavailable in Navit's non-GLib configurations,
so enabling it in general builds would break supported targets.

Before any production switch, implement the following:

* Replace the GLib-only path representation with platform-compatible
  containers or suitable project abstractions.
* Support start positions on a partial road segment and multiple
  destination candidates with nonzero partial-road terminal costs.
* Express waypoints and temporary turnaround penalties correctly.
* Preserve restriction-clone identities and real map-path geometries.
* Integrate a state-aware route extraction path without falling back
  to the point-global successor.
* Replace full per-query search with a correct incremental state
  update strategy, or explicitly justify initial full recomputation.
* Run geographic regression tests and measure latency and memory.

The existing production Red cases are intentionally unchanged. They
must not be considered fixed because a separate candidate passes.

## Development sequence

1. **Red**: Preserve the reproducible compiled production failures and
   add CORE-004B so a single-global-successor patch cannot pass.
2. **Green A**: Implement and test state-aware shortest-path calculations
   without changing public route extraction. This establishes an
   independent C implementation against real `route_graph_segment`
   objects, *not* a complete Green result.
3. **Green B**: Connect route extraction and update propagation to the
   state-aware results. Only then can CORE-004, CORE-004B, and INCR-006
   all turn Green on production behavior.
4. **Refactor**: Factor reusable transition-cost calculation and
   incremental state invalidation while retaining behavior.
5. **Performance**: Measure overhead and memory cost, compare with the
   original solution and investigate selective state expansion.

**Current status:** Production CORE-004 and INCR-006 are Red. No
production routing semantics have been changed. Failing CI is expected
until a complete state-aware implementation passes the accepted tests.
