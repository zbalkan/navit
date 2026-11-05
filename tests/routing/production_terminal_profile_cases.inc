/* Terminal selection, arithmetic saturation and restricted-origin policy. */
static int navit_routing_test_terminal_profile(int case_id) {
    struct route_graph *graph = g_new0(struct route_graph, 1);
    struct vehicleprofile profile = { 0 };
    struct roadprofile road_profile = { 0 };
    struct route_graph_point *a, *b, *c, *d;
    struct route_graph_segment *open, *restricted, *ab;
    struct route_graph_segment_data data = { 0 };
    struct item item = { 0 };
    struct coord coords[] = { { 811, 811 }, { 812, 811 }, { 813, 811 }, { 814, 811 } };
    int result = 0, expected = 0, actual = 0, original = 0;

    graph->heap = fh_makekeyheap();
    profile.roadprofile_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
    profile.through_traffic_penalty = 100;
    road_profile.speed = 36;
    road_profile.maxspeed = 36;
    g_hash_table_insert(profile.roadprofile_hash, (gpointer)(long)type_street_0, &road_profile);
    a = route_graph_add_point(graph, &coords[0]);
    b = route_graph_add_point(graph, &coords[1]);
    c = route_graph_add_point(graph, &coords[2]);
    d = route_graph_add_point(graph, &coords[3]);
    item.type = type_street_0;
    data.item = &item;
    data.maxspeed = 36;
    data.flags = AF_SPEED_LIMIT;
    data.len = 1;

    if (case_id == 43 || case_id == 44) {
        item.id_lo = 101;
        route_graph_add_segment(graph, b, c, &data);
        open = b->start;
        if (case_id == 43) {
            item.id_lo = 102;
            data.flags |= AF_THROUGH_TRAFFIC_LIMIT;
            data.len = 3;
            route_graph_add_segment(graph, b, d, &data);
            restricted = b->start;
            route_graph_seed_destination(graph, &profile, b, open, 1, 100);
            route_graph_seed_destination(graph, &profile, b, restricted, 3, 100);
            route_graph_compute_shortest_path(graph, &profile, NULL);
            actual = b->restricted_value;
            expected = 3;
            result = actual == expected && b->value == 1
                     && b->dst_seg == open && b->restricted_dst_seg == restricted;
        } else {
            route_graph_seed_destination(graph, &profile, b, open, INT_MAX - 5, 100);
            actual = b->restricted_dst_val;
            expected = INT_MAX;
            result = actual == expected && b->restricted_el == NULL;
        }
    } else if (case_id == 45) {
        data.flags |= AF_THROUGH_TRAFFIC_LIMIT;
        item.id_lo = 103;
        route_graph_add_segment(graph, a, b, &data);
        ab = a->start;
        data.flags = AF_SPEED_LIMIT;
        item.id_lo = 104;
        route_graph_add_segment(graph, b, c, &data);
        item.id_lo = 105;
        data.flags |= AF_THROUGH_TRAFFIC_LIMIT;
        data.len = 2;
        route_graph_add_segment(graph, b, d, &data);
        item.id_lo = 106;
        data.flags = AF_SPEED_LIMIT;
        data.len = 1;
        route_graph_add_segment(graph, d, c, &data);
        route_graph_seed_destination(graph, &profile, c, NULL, 0, 0);
        route_graph_compute_shortest_path(graph, &profile, NULL);
        actual = a->value;
        expected = 102;
        result = actual == expected && a->seg == ab;

        profile.through_traffic_penalty = 0;
        route_graph_reset(graph);
        route_graph_seed_destination(graph, &profile, c, NULL, 0, 0);
        route_graph_compute_shortest_path(graph, &profile, NULL);
        original = a->value;
        result = result && original == 2 && actual - original == 100;
    }
    if (!result)
        fprintf(stderr, "Terminal/profile case %d: expected=%d got=%d unpenalized=%d\n",
                case_id, expected, actual, original);
    route_graph_free_segments(graph);
    route_graph_free_points(graph);
    fh_deleteheap(graph->heap);
    g_hash_table_destroy(profile.roadprofile_hash);
    g_free(graph);
    return result ? 0 : 1;
}
