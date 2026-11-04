#include "production_hooks.h"

/*
 * Benchmark input is a deterministic synthetic routing graph, not a map.
 * Both search implementations operate on the same live Navit segments.
 * Times measure search/repair, excluding graph construction. The
 * candidate currently performs full Dijkstra recomputation on updates.
 */
int navit_routing_benchmark(int n, int repeats, int shortcuts) {
    struct route_graph *graph = g_new0(struct route_graph, 1);
    struct vehicleprofile profile = { 0 };
    struct roadprofile road_profile = { 0 };
    struct route_graph_point **points = g_new0(struct route_graph_point *, n);
    struct route_graph_segment *changed = NULL;
    struct route_graph_segment_data data = { 0 };
    struct item road_item = { 0 };
    struct route_turn_path turn_path = { 0 };
    struct coord point_coord;
    gint64 begin, end;
    int i, repetition, baseline, candidate;
    int edges = 0;
    int failed = 0;

    graph->heap = fh_makekeyheap();
    profile.roadprofile_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
    road_profile.speed = 36;
    road_profile.maxspeed = 36;
    g_hash_table_insert(profile.roadprofile_hash, (gpointer)(long)type_street_0, &road_profile);
    road_item.type = type_street_0;
    data.item = &road_item;
    data.flags = AF_SPEED_LIMIT | AF_ONEWAY;
    data.maxspeed = 36;

    for (i = 0; i < n; i++) {
        point_coord.x = i + 100000;
        point_coord.y = 100000;
        points[i] = route_graph_add_point(graph, &point_coord);
    }
    for (i = 0; i < n - 1; i++) {
        data.len = 10;
        road_item.id_lo = i + 1;
        route_graph_add_segment(graph, points[i], points[i + 1], &data);
        if (i == n / 2)
            changed = points[i]->start;
        edges++;
    }
    if (shortcuts) {
        for (i = 0; i < n - 2; i++) {
            data.len = 21;
            road_item.id_lo = n + i;
            route_graph_add_segment(graph, points[i], points[i + 2], &data);
            edges++;
        }
    }

    for (repetition = 0; repetition < repeats; repetition++) {
        changed->data.len = 10;
        route_graph_reset(graph);
        points[n - 1]->dst_val = 0;
        points[n - 1]->rhs = 0;
        points[n - 1]->el = fh_insertkey(graph->heap, 0, points[n - 1]);

        begin = g_get_monotonic_time();
        route_graph_compute_shortest_path(graph, &profile, NULL);
        end = g_get_monotonic_time();
        baseline = points[0]->value;
        printf("%s,%d,%d,%d,legacy,full,%lld,%d\n", shortcuts ? "skip" : "chain", n, edges,
               repetition, (long long)((end - begin) * 1000), baseline);

        begin = g_get_monotonic_time();
        candidate = route_turn_graph_path(graph, &profile, points[0], points[n - 1], &turn_path)
                    ? turn_path.cost : INT_MAX;
        end = g_get_monotonic_time();
        printf("%s,%d,%d,%d,movement,full,%lld,%d\n", shortcuts ? "skip" : "chain", n, edges,
               repetition, (long long)((end - begin) * 1000), candidate);
        if (baseline != candidate)
            failed = 1;
        route_turn_path_clear(&turn_path);

        changed->data.len = 30;
        begin = g_get_monotonic_time();
        route_graph_point_update(&profile, changed->start, graph->heap);
        route_graph_compute_shortest_path(graph, &profile, NULL);
        end = g_get_monotonic_time();
        baseline = points[0]->value;
        printf("%s,%d,%d,%d,legacy,update,%lld,%d\n", shortcuts ? "skip" : "chain", n, edges,
               repetition, (long long)((end - begin) * 1000), baseline);

        begin = g_get_monotonic_time();
        candidate = route_turn_graph_path(graph, &profile, points[0], points[n - 1], &turn_path)
                    ? turn_path.cost : INT_MAX;
        end = g_get_monotonic_time();
        printf("%s,%d,%d,%d,movement,update,%lld,%d\n", shortcuts ? "skip" : "chain", n, edges,
               repetition, (long long)((end - begin) * 1000), candidate);
        if (baseline != candidate)
            failed = 1;
        route_turn_path_clear(&turn_path);
        if (failed)
            break;
    }

    if (failed)
        fprintf(stderr, "ERROR benchmark algorithms disagree: n=%d shortcuts=%d\n", n, shortcuts);

    route_graph_free_segments(graph);
    route_graph_free_points(graph);
    fh_deleteheap(graph->heap);
    g_hash_table_destroy(profile.roadprofile_hash);
    g_free(points);
    g_free(graph);
    return failed;
}
