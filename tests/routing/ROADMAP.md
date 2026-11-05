# Navit routing correctness — consolidated roadmap

Status: **work in progress; not merge-ready**. Updated 5 November 2026 following test-first implementation and single-heap validation.

## Scope and branch policy

**Baseline:** `feat/routing-turn-state-green` at `a2b60d489fe2`.

**Single production correction branch:** `fix/routing-through-traffic-arrival-states`. Cases 4, 5 and 6 share the same through-traffic state-space defect and belong in one PR. The branch starts with a squashed snapshot of the prior two-label prototype; it is **not** the finished design.

**Independent warning fix:** `fix/routing-benchmark-uninitialized-cost`, already implemented and CI-green; merge independently.

**Superseded branches:** `fix/routing-arrival-context-paths`, `fix/routing-predecessor-dependent-penalty`, `fix/routing-incremental-successor-change` were deleted by the repository owner after their common implementation was consolidated. Do not recreate overlapping fixes.

**Separate future concern:** `fix/vehicleprofile-penalty-validation` for negative `through_traffic_penalty` configuration (D4). An origin-zone exemption (D1-c) is a separate product decision, not required for the correction.

## Problem and evidence

Navit's LPA*-derived routing stores a single cost and successor at each junction, but the cost of leaving a through-traffic-restricted segment depends on the *incoming* road class. Therefore different arrivals at the same junction can require different optimal successors. The shipped `vehicleprofile.c` default is `through_traffic_penalty = 9000` (tenths of seconds); this is the normal production path, not a rare opt-in.

| Fixture | Old observed | Correct | Failure and mandatory test |
| --- | ---: | ---: | --- |
| 1–3 | Pass | Unchanged | Baseline behavior with penalty 0 |
| 4 / CORE-004 | A=103 | A=4 | Restricted arrival must choose restricted B→D, not cheaper unrestricted B→C |
| 5a / INCR-006 | Incremental=3; fresh legacy=103 | Incremental=scratch | Test *incremental consistency* independently of the external oracle |
| 5b / INCR-006 | A=3 after update | A=4 | Test final correct cost under the arrival-aware recurrence |
| 6 / CORE-004B | A=103, X=3 | A=4, X=3 | A and X require distinct successors at the same junction |
| 26–28 | Pass | Unchanged | Experimental candidate terminal transitions and negative continuity checks |

Fixture costs use P=100 and the test roads' small integer travel costs. Add the production default P=9000 to every new correctness comparison; do not mechanically reuse expected cost 4 for every constructed scenario without an independent oracle.

**Evidence correction:** The previous 1,500-row benchmark used `profile = {0}`, so it ran with P=0 and did **not** measure or validate the two-label production path. The previous sanitizer and regression runs are useful but do not establish default-profile correctness.

## Target design

Retain two labels per junction for the **through-traffic penalty subproblem only**: **U** (arrived on unrestricted road) and **R** (arrived on restricted road). Their transition costs depend on the outgoing road's class. **This reduced state space cannot enforce arbitrary incoming-segment turn rules**: prohibiting exact physical-segment reversals and distinguishing distinct segments with the same road item requires an incoming-movement state. D3 therefore remains a blocker to using the two-label recurrence as a fully equivalent production router. Preserve the separate movement-state solver as the semantic reference; do not claim full production equivalence until exact-turn behavior is verified against an incoming-segment oracle.

Replace the prototype's **two separate Fibonacci heaps** with a **single globally key-ordered heap** whose elements identify their arrival class. Each label holds `value`, `rhs`, `seg`, `el` and, only if justified by reversal tests, a successor-dirty flag. Store both labels in one `route_graph_state state[2]` array. Remove `route_graph_point::graph`, `restricted_heap` and redundant per-point dirty fields.

Seed both terminal classes in `route_graph_init()`; eliminate the full hash-table scan on every recomputation. When a state changes, update only predecessor transitions that lead into the changed arrival class, while recomputing both labels on each affected predecessor. Drain the single heap by `min(g, rhs)` and validate the equal-key policy. Avoid retaining a duplicate legacy propagation algorithm for P=0.

Preserve the existing graph filtering, restriction expansion, compact segment allocation, traffic-change invalidation, route geometry reuse and path-extraction interfaces wherever their invariants remain valid.

## Semantic decisions

| ID | Decision for this PR | Test or follow-up |
| --- | --- | --- |
| D1 | **Provisional D1-b:** the first road's class determines the initial arrival label; a restricted origin incurs P when its first restricted-to-unrestricted transition occurs | Explicit origin-inside-restricted-zone test; check all ETA and path-time consumers before approval |
| D2 | Ignore graph segments where `start == end` in successor updates | Named self-loop fixture |
| D3 | Immediate reversal on the exact same segment remains forbidden; matching item on another segment may incur `turn_around_penalty2` | Named reversal and same-item fixtures with penalty2=50 |
| D4 | Negative through-traffic penalties are invalid configuration; validate at profile loading | Small independent branch, warning and rejection/clamping policy to finalize |
| D5 | Restricted arrival to an open terminal segment incurs P | Named terminal transition fixture |

D1-c (free exit from the zone containing the origin) needs an additional origin-zone state and is explicitly **deferred**, pending a product decision. D1-b must be documented as a behavior change; do not silently infer that it exactly matches legal `access=destination` semantics.

## Implementation sequence — Red / Green / Refactor

| Step | Change | Exit condition |
| --- | --- | --- |
| R1 | Split case 5 into incremental-vs-scratch (5a) and independent oracle (5b) | Both fail for the old implementation for the intended reasons; 1–3 remain green |
| R2 | Add compiled independent Dijkstra oracle over (junction, incoming segment) and deterministic graph generator | Repeatable mismatches with seed, graph, penalty and direction printed |
| R3 | Extract segment base-cost function and isolate successor-dependent checks | Baseline P=0 results unchanged |
| R4 | Introduce `route_graph_state[2]` and a common state accessor | Compiles across supported targets; `sizeof` recorded |
| R5 | Implement single-heap, key-ordered incremental propagation and destination seeding | 4, 5a, 5b, 6 and all differential labels green |
| R6 | Extract paths according to arrival class and D1-b | Path cost, direction and continuity agree with the independent oracle |
| R7 | Restore D2/D3 behavior, test D5 and integer saturation | All named semantic fixtures green under sanitizers |
| R8 | Benchmark with P=9000 and create representative OSM/maptool workload | Artifact includes timings, heap extractions, environment and memory; no performance conclusion without comparison |
| R9 | Document recurrence, memory, compatibility and behavioral changes | Minimal focused PR with all green evidence attached |

The branch began with a prototype snapshot. R1 has separate case 5a and oracle assertions; R2 has a compiled independent reverse-Dijkstra oracle over the two arrival classes (6,000 graph/penalty trials) plus 384 cost-change incremental-versus-scratch trials. This is narrower than V1's full incoming-segment state oracle and V2's 1–5 event batches. R3 extracted the common segment cost. R5 now uses a single globally ordered queue for P>0, class-targeted predecessor repairs and direct terminal seeding, but a separate legacy path remains for P=0 and the target `state[2]` layout is not yet adopted. R7 includes Red-to-Green self-loop and exact-reversal fixtures. These are **intermediate milestones**, not all acceptance criteria.

## Mandatory validation

**V1 — Compiled differential oracle:** At least 2,000 deterministic seeds, graph sizes 4–200, restrictions 10%/30%/60%, one-way 0%/20%, P=0/100/9000 and `turn_around_penalty2`=0/50. For every reachable (junction, class) compare solver cost against an independently implemented forward Dijkstra over incoming-segment states. Check extracted path continuity and its independently recomputed cost. Oracle must not call `route_turn_*` or `route_arrival_*`.

**V2 — Incremental consistency:** Apply batches of 1–5 changes (cost increase, decrease, closure, reopen, traffic distortion), recompute incrementally, then compare every class label and successor-policy cost against a fresh flood of the same graph. Include zero-length/rounded-to-zero edges, and diagnose any stale cycles before changing the cost model.

**V3 — Fixed regressions:** Cases 1–6, split 5a/5b, D1–D3 and D5 named tests, experimental candidate 26–28 and negative path-continuity checks. Test both P=100 and P=9000.

**V4 — Performance evidence:** On P=9000 compare baseline and fix for full graph flood, incremental updates and heap extraction counts. Measure graph memory and a real OSM-derived map with restricted roads. The review proposed extraction ≤2.5× baseline and initial flood wall time ≤2×, but **neither budget is approved**; report measurements before agreeing on thresholds.

**V5 — Quality gates:** ASan+UBSan, full cross-platform Build Navit Binaries, independent oracle, production regression, movement-state candidate and routing benchmark. Include `sizeof(struct route_graph_point)` for each usable toolchain and link all workflows to the **same PR head commit**. Avoid treating earlier-commit green runs as HEAD evidence.

## Acceptance and sequencing

The routing PR is merge-ready only after V1–V5 pass at its final commit, D1 and performance budgets receive explicit approval, and no divergence appears between incremental and fresh results. CI must use the default P=9000 as well as P=100; passing state-aware production fixture cases 4–6 alone are insufficient.

Merge the benchmark-warning fix independently. Then land the consolidated routing PR after its verification. Address D4 through a separate small change. Consider D1-c only if requested. The superseded fix branches have been deleted by the repository owner. Retain `feat/routing-turn-state-green` while this work depends on its baseline.

## Key sources

- User-supplied review, *Navit through-traffic routing: corrected fix direction* (5 November 2026), of baseline `a2b60d48` and prototype `8a46fbf3`; the review document is not stored in this repository.
- `navit/route.c`, `navit/route_protected.h`, `navit/route_arrival_state.inc`, `navit/vehicleprofile.c`
- `tests/routing/production_penalty_cases.inc`, `tests/routing/benchmark_production.inc`
- [Baseline](https://github.com/zbalkan/navit/commit/a2b60d489fe230e43b4e64d2e0d1b36a5108a638)
- [Consolidated fix branch](https://github.com/zbalkan/navit/tree/fix/routing-through-traffic-arrival-states)
