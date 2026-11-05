/* Focused production-semantics tests for self-loops and exact reversals. */
static int navit_routing_test_arrival_semantics(int case_id) {
    struct route_graph *graph = g_new0(struct route_graph, 1);
    struct vehicleprofile profile = { 0 };
    struct roadprofile road_profile = { 0 };
    struct route_graph_point *a, *b, *c;
    struct route_graph_segment *first, *other;
    struct route_graph_segment_data data = { 0 };
    struct item item = { 0 };
    struct coord coords[] = { { 501, 501 }, { 502, 501 }, { 503, 501 } };
    int result = 0;
    int actual, expected;

    graph->heap = fh_makekeyheap();
    profile.roadprofile_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
    profile.through_traffic_penalty = 100;
    profile.turn_around_penalty2 = 50;
    road_profile.speed = 36;
    road_profile.maxspeed = 36;
    g_hash_table_insert(profile.roadprofile_hash, (gpointer)(long)type_street_0, &road_profile);

    a = route_graph_add_point(graph, &coords[0]);
    b = route_graph_add_point(graph, &coords[1]);
    c = route_graph_add_point(graph, &coords[2]);
    item.type = type_street_0;
    item.id_lo = 101;
    data.item = &item;
    data.flags = AF_SPEED_LIMIT;
    data.maxspeed = 36;
    data.len = 1;

    if (case_id == 35) {
        /* The self-loop must not enter the successor search. */
        route_graph_add_segment(graph, a, a, &data);
        first = a->start;
        a->value = 0;
        actual = route_arrival_step(&profile, first, 1, a, 0);
        expected = INT_MAX;
        result = actual == expected;
    } else if (case_id == 36) {
        /* Exact segment reversal is forbidden even if penalty2 > 0. */
        route_graph_add_segment(graph, a, b, &data);
        first = a->start;
        b->seg = first;
        b->value = 0;
        actual = route_arrival_step(&profile, first, 1, b, 0);
        expected = INT_MAX;
        result = actual == expected;

        /* Another segment with the same item may incur penalty2. */
        route_graph_add_segment(graph, b, c, &data);
        other = b->start;
        b->seg = other;
        actual = route_arrival_step(&profile, first, 1, b, 0);
        if (result) {
            expected = 51;
            result = actual == expected;
        }
    } else {
        expected = -1;
        actual = -1;
    }

    if (!result)
        fprintf(stderr, "Arrival semantic case %d: expected=%d actual=%d\n",
                case_id, expected, actual);
    route_graph_free_segments(graph);
    route_graph_free_points(graph);
    fh_deleteheap(graph->heap);
    g_hash_table_destroy(profile.roadprofile_hash);
    g_free(graph);
    return result ? 0 : 1;
}
