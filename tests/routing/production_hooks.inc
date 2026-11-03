/* Test-only hooks compiled in the production route.c translation unit.
 * Never compiled unless NAVIT_ROUTING_TEST_HOOKS is enabled.
 * These tests exercise actual Navit update/propagation functions.
 */
int navit_routing_test_case(int case_id) {
    struct route_graph *graph = g_new0(struct route_graph, 1);
    struct vehicleprofile profile = { 0 };
    struct roadprofile road_profile = { 0 };
    struct route_graph_point *a, *b, *c, *d;
    struct route_graph_segment *ab, *bd, *ac, *cd;
    struct route_graph_segment_data data = { 0 };
    struct item road_item = { 0 };
    struct coord coords[] = { { 10, 10 }, { 20, 10 }, { 10, 20 }, { 20, 20 } };
    int observed, expected, result;

    graph->heap = fh_makekeyheap();
    profile.roadprofile_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
    road_profile.speed = 36;
    road_profile.maxspeed = 36;
    g_hash_table_insert(profile.roadprofile_hash, (gpointer)(long)type_street_0, &road_profile);

    a = route_graph_add_point(graph, &coords[0]);
    b = route_graph_add_point(graph, &coords[1]);
    c = route_graph_add_point(graph, &coords[2]);
    d = route_graph_add_point(graph, &coords[3]);

    road_item.type = type_street_0;
    data.item = &road_item;
    data.flags = AF_SPEED_LIMIT;
    data.maxspeed = 36;
    data.len = 2;
    road_item.id_lo = 1;
    route_graph_add_segment(graph, a, b, &data);
    ab = a->start;
    data.len = 3;
    road_item.id_lo = 2;
    route_graph_add_segment(graph, b, d, &data);
    bd = b->start;
    data.len = 4;
    road_item.id_lo = 3;
    route_graph_add_segment(graph, a, c, &data);
    ac = a->start;
    data.len = 4;
    road_item.id_lo = 4;
    route_graph_add_segment(graph, c, d, &data);
    cd = c->start;

    /* Seed one zero-cost terminal; exercise production propagation. */
    d->dst_val = 0;
    d->rhs = 0;
    d->el = fh_insertkey(graph->heap, 0, d);
    route_graph_compute_shortest_path(graph, &profile, NULL);

    /* Expected costs are in Navit's tenths-of-seconds metric:
     * at 36 km/h each meter corresponds to one tenth of a second.
     */
    if (case_id == 1) {
        expected = 5;
        observed = a->value;
        result = observed == expected && a->seg == ab && b->seg == bd
                 && a->value == a->rhs && !fh_min(graph->heap);
    } else if (case_id == 2) {
        bd->data.len = 10;
        route_graph_point_update(&profile, b, graph->heap);
        route_graph_compute_shortest_path(graph, &profile, NULL);
        expected = 8;
        observed = a->value;
        result = observed == expected && a->seg == ac && c->seg == cd
                 && a->value == a->rhs && !fh_min(graph->heap);
    } else if (case_id == 3) {
        bd->data.len = 10;
        route_graph_point_update(&profile, b, graph->heap);
        route_graph_compute_shortest_path(graph, &profile, NULL);
        bd->data.len = 1;
        route_graph_point_update(&profile, b, graph->heap);
        route_graph_compute_shortest_path(graph, &profile, NULL);
        expected = 3;
        observed = a->value;
        result = observed == expected && a->seg == ab && b->seg == bd
                 && a->value == a->rhs && !fh_min(graph->heap);
    } else {
        expected = -1;
        observed = -1;
        result = 0;
    }

    if (!result)
        fprintf(stderr, "Navit routing production case %d: got %d, expected %d\n",
                case_id, observed, expected);

    route_graph_free_segments(graph);
    route_graph_free_points(graph);
    fh_deleteheap(graph->heap);
    g_hash_table_destroy(profile.roadprofile_hash);
    g_free(graph);
    return result ? 0 : 1;
}
