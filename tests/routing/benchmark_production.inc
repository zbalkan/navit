#include "production_hooks.h"
#include <time.h>

/*
 * Deterministic, graph-backed comparison.  Both implementations search
 * the same Navit graph, while the cost oracle is independent of either
 * algorithm: dynamic programming on this forward-only chain/DAG.
 *
 * Times exclude graph construction, reset, oracle evaluation and output.
 * The movement-state candidate recomputes the route on every update;
 * it does not perform incremental repair.
 */
/* Prefer a nanosecond-resolution monotonic clock when supported. */
static long long routing_benchmark_now_ns(void) {
#if defined(CLOCK_MONOTONIC)
    struct timespec timestamp;
    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) == 0)
        return (long long)timestamp.tv_sec * 1000000000LL + timestamp.tv_nsec;
#endif
    return (long long)g_get_monotonic_time() * 1000LL;
}

static int routing_benchmark_oracle(int n, int shortcuts, int changed_len) {
    int *distance = g_new0(int, n);
    int i, answer;
    distance[n - 1] = 0;
    for (i = n - 2; i >= 0; --i) {
        int edge = i == n / 2 ? changed_len : 10;
        int best = edge + distance[i + 1];
        if (shortcuts && i + 2 < n && 21 + distance[i + 2] < best)
            best = 21 + distance[i + 2];
        distance[i] = best;
    }
    answer = distance[0];
    g_free(distance);
    return answer;
}

static int routing_benchmark_measure(struct route_graph *graph,
                                     struct vehicleprofile *profile,
                                     struct route_graph_point *start,
                                     struct route_graph_point *goal,
                                     struct route_graph_point *changed_start,
                                     const char *scenario, int n, int edges,
                                     int repetition, const char *operation,
                                     int expected, int reverse_order, int emit) {
    struct route_turn_path turn_path = {0};
    long long begin, end;
    long long elapsed[2] = { 0, 0 };
    int cost[2] = { INT_MAX, INT_MAX };
    int order, which;
    int good;

    for (order = 0; order < 2; ++order) {
        /* Alternate measurement order between iterations to reduce bias. */
        which = reverse_order ? 1 - order : order;
        begin = routing_benchmark_now_ns();
        if (which == 0) {
            /* Measure notification and repair together for changed edges. */
            if (changed_start)
                route_graph_point_update(profile, changed_start, graph->heap);
            route_graph_compute_shortest_path(graph, profile, NULL);
            cost[which] = start->value;
        } else {
            cost[which] = route_turn_graph_path(graph, profile, start, goal, &turn_path)
                          ? turn_path.cost : INT_MAX;
            route_turn_path_clear(&turn_path);
        }
        end = routing_benchmark_now_ns();
        elapsed[which] = end - begin;
    }

    good = cost[0] == expected && cost[1] == expected;
    /* Even a warm-up mismatch must leave a machine-readable failure row. */
    if (emit || !good) {
        printf("%s,%d,%d,%d,legacy,%s,%lld,%d,%d,%s\n",
               scenario, n, edges, repetition, operation, elapsed[0], cost[0],
               expected, cost[0] == expected ? "pass" : "fail");
        printf("%s,%d,%d,%d,movement,%s,%lld,%d,%d,%s\n",
               scenario, n, edges, repetition, operation, elapsed[1], cost[1],
               expected, cost[1] == expected ? "pass" : "fail");
    }
    if (!good)
        fprintf(stderr,
                "ERROR routing benchmark %s n=%d repetition=%d operation=%s: "
                "oracle=%d legacy=%d movement=%d\n",
                scenario, n, repetition, operation, expected, cost[0], cost[1]);
    return good ? 0 : 1;
}

int navit_routing_benchmark(int n, int repeats, int shortcuts) {
    struct route_graph *graph = g_new0(struct route_graph, 1);
    struct vehicleprofile profile = {0};
    struct roadprofile road_profile = {0};
    struct route_graph_point **points = g_new0(struct route_graph_point *, n);
    struct route_graph_segment *changed = NULL;
    struct route_graph_segment_data data = {0};
    struct item road_item = {0};
    struct coord point_coord;
    const char *scenario = shortcuts ? "skip" : "chain";
    int i, repetition, edges = 0, failed = 0;

    graph->heap = fh_makekeyheap();
    profile.roadprofile_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
    road_profile.speed = 36;
    road_profile.maxspeed = 36;
    g_hash_table_insert(profile.roadprofile_hash, (gpointer)(long)type_street_0, &road_profile);
    road_item.type = type_street_0;
    data.item = &road_item;
    data.flags = AF_SPEED_LIMIT | AF_ONEWAY;
    data.maxspeed = 36;

    for (i = 0; i < n; ++i) {
        point_coord.x = i + 100000;
        point_coord.y = 100000;
        points[i] = route_graph_add_point(graph, &point_coord);
    }
    for (i = 0; i < n - 1; ++i) {
        data.len = 10;
        road_item.id_lo = i + 1;
        route_graph_add_segment(graph, points[i], points[i + 1], &data);
        if (i == n / 2)
            changed = points[i]->start;
        ++edges;
    }
    if (shortcuts) {
        for (i = 0; i < n - 2; ++i) {
            data.len = 21;
            road_item.id_lo = n + i;
            route_graph_add_segment(graph, points[i], points[i + 2], &data);
            ++edges;
        }
    }

    /* Three untimed warm-up iterations followed by paired samples. */
    for (repetition = -3; repetition < repeats; ++repetition) {
        int emit = repetition >= 0;
        int reverse_order = (repetition & 1) != 0;
        changed->data.len = 10;
        route_graph_reset(graph);
        points[n - 1]->dst_val = 0;
        points[n - 1]->rhs = 0;
        points[n - 1]->el = fh_insertkey(graph->heap, 0, points[n - 1]);

        failed |= routing_benchmark_measure(graph, &profile, points[0], points[n - 1],
                                            NULL, scenario, n, edges, repetition, "full",
                                            routing_benchmark_oracle(n, shortcuts, 10),
                                            reverse_order, emit);
        if (failed)
            break;

        changed->data.len = 30;
        failed |= routing_benchmark_measure(graph, &profile, points[0], points[n - 1],
                                            changed->start, scenario, n, edges, repetition, "increase",
                                            routing_benchmark_oracle(n, shortcuts, 30),
                                            reverse_order, emit);
        if (failed)
            break;

        changed->data.len = 10;
        failed |= routing_benchmark_measure(graph, &profile, points[0], points[n - 1],
                                            changed->start, scenario, n, edges, repetition, "decrease",
                                            routing_benchmark_oracle(n, shortcuts, 10),
                                            reverse_order, emit);
        if (failed)
            break;
    }

    route_graph_free_segments(graph);
    route_graph_free_points(graph);
    fh_deleteheap(graph->heap);
    g_hash_table_destroy(profile.roadprofile_hash);
    g_free(points);
    g_free(graph);
    return failed;
}
