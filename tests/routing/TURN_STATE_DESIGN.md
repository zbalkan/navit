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
