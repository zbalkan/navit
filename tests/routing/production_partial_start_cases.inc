/* Partial-road start selection using the production graph and cost model. */
static int navit_routing_test_partial_start(int case_id) {
    struct route_graph *graph = g_new0(struct route_graph, 1);
    struct vehicleprofile profile = { 0 };
    struct roadprofile road_profile = { 0 };
    struct route_graph_point *a, *b, *c;
    struct route_graph_segment *ab, *bc, *ac;
    struct route_graph_segment_data data = { 0 };
    struct item road_item = { 0 };
    struct route_turn_path plan = { 0 };
    struct coord coords[] = { { 31001, 31001 }, { 31002, 31001 }, { 31003, 31001 } };
    int expected, expected_direction, result;

    graph->heap = fh_makekeyheap();
    profile.roadprofile_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
    profile.flags_forward_mask = AF_ONEWAYREV;
    profile.flags_reverse_mask = AF_ONEWAY;
    road_profile.speed = 36;
    road_profile.maxspeed = 36;
    g_hash_table_insert(profile.roadprofile_hash, (gpointer)(long)type_street_0, &road_profile);

    a = route_graph_add_point(graph, &coords[0]);
    b = route_graph_add_point(graph, &coords[1]);
    c = route_graph_add_point(graph, &coords[2]);

    road_item.type = type_street_0;
    road_item.id_lo = 700;
    data.item = &road_item;
    data.flags = AF_SPEED_LIMIT;
    if (case_id == 20)
        data.flags |= AF_ONEWAY;
    data.maxspeed = 36;
    data.len = 10;
    route_graph_add_segment(graph, a, b, &data);
    ab = a->start;

    road_item.id_lo = 701;
    data.flags = AF_SPEED_LIMIT;
    data.len = 1;
    route_graph_add_segment(graph, b, c, &data);
    bc = b->start;

    road_item.id_lo = 702;
    data.len = 4;
    route_graph_add_segment(graph, a, c, &data);
    ac = a->start;

    c->dst_val = 0;
    if (case_id == 21)
        profile.turn_around_penalty = 4;
    result = route_turn_plan_from_segment(graph, &profile, ab, 25, case_id == 21 ? 1 : 0, &plan);

    if (case_id == 19) {
        expected = 6;
        expected_direction = -1;
    } else {
        expected = 8;
        expected_direction = 1;
    }
    result = result && plan.cost == expected && plan.terminal == c
             && plan.len == 2 && plan.segments[0] == ab
             && plan.directions[0] == expected_direction
             && plan.segments[1] == (expected_direction > 0 ? bc : ac)
             && plan.directions[1] == 1;
    if (!result)
        fprintf(stderr, "Partial-road start case %d: cost=%d expected=%d direction=%d expected_dir=%d\n",
                case_id, plan.cost, expected, plan.len ? plan.directions[0] : 0, expected_direction);

    route_turn_path_clear(&plan);
    route_graph_free_segments(graph);
    route_graph_free_points(graph);
    fh_deleteheap(graph->heap);
    g_hash_table_destroy(profile.roadprofile_hash);
    g_free(graph);
    return result ? 0 : 1;
}
