/* Regression: partial destination traversal is an outgoing movement.
 * A restricted incoming movement must pay the through-traffic penalty
 * if it exits the restricted network along an unrestricted terminal road.
 */
static int navit_routing_test_terminal_transition(int case_id) {
    struct route_graph *graph = g_new0(struct route_graph, 1);
    struct vehicleprofile profile = {0};
    struct roadprofile road_profile = {0};
    struct route_graph_point *a, *b, *c, *d, *goal;
    struct route_graph_segment *ab, *bc, *dg, *bd;
    struct route_graph_segment_data data = {0};
    struct item road_item = {0};
    struct route_turn_path path = {0};
    struct coord coords[] = {{41001, 41001}, {41002, 41001}, {41003, 41001},
                             {41004, 41001}, {41005, 41001}};
    int result;

    graph->heap = fh_makekeyheap();
    profile.roadprofile_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
    profile.through_traffic_penalty = 100;
    profile.flags_forward_mask = AF_ONEWAYREV;
    profile.flags_reverse_mask = AF_ONEWAY;
    road_profile.speed = 36;
    road_profile.maxspeed = 36;
    g_hash_table_insert(profile.roadprofile_hash, (gpointer)(long)type_street_0, &road_profile);

    a = route_graph_add_point(graph, &coords[0]);
    b = route_graph_add_point(graph, &coords[1]);
    c = route_graph_add_point(graph, &coords[2]);
    d = route_graph_add_point(graph, &coords[3]);
    goal = route_graph_add_point(graph, &coords[4]);

    road_item.type = type_street_0;
    data.item = &road_item;
    data.maxspeed = 36;
    data.len = 1;
    data.flags = AF_SPEED_LIMIT | AF_THROUGH_TRAFFIC_LIMIT;

    road_item.id_lo = 1001;
    route_graph_add_segment(graph, a, b, &data);
    ab = a->start;

    road_item.id_lo = 1002;
    data.len = 2;
    route_graph_add_segment(graph, b, d, &data);
    bd = b->start;

    road_item.id_lo = 1003;
    data.len = 1;
    data.flags = AF_SPEED_LIMIT;
    route_graph_add_segment(graph, b, c, &data);
    bc = b->start;

    road_item.id_lo = 1004;
    data.flags = AF_SPEED_LIMIT | AF_THROUGH_TRAFFIC_LIMIT;
    route_graph_add_segment(graph, d, goal, &data);
    dg = d->start;

    if (case_id == 27)
        b->dst_seg = ab; /* reversal of the exact incoming road */
    else
        b->dst_seg = bc;
    if (case_id == 28)
        bc->data.flags |= AF_ONEWAYREV; /* disallow forward terminal movement */
    b->dst_val = 1;
    d->dst_seg = dg;
    d->dst_val = 1;

    result = route_turn_graph_path_to_terminals(graph, &profile, a, &path);
    result = result && path.cost == 4 && path.terminal == d
             && path.terminal_segment == dg && path.terminal_direction == 1
             && path.len == 2 && path.segments[1] == bd;
    if (!result)
        fprintf(stderr, "terminal movement case %d: expected terminal D cost 4; got cost=%d terminal=%s\n",
                case_id, path.cost, path.terminal == b ? "B" : path.terminal == d ? "D" : "other");

    route_turn_path_clear(&path);
    route_graph_free_segments(graph);
    route_graph_free_points(graph);
    fh_deleteheap(graph->heap);
    g_hash_table_destroy(profile.roadprofile_hash);
    g_free(graph);
    return result ? 0 : 1;
}
