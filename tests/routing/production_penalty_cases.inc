/* CORE-004 and INCR-006: production shortest-path tests using actual
 * route_value_seg(), route_graph_point_update(), and LPA* propagation.
 * These fixtures express costs in tenths of a second at 36 km/h.
 *
 * Their oracle costs are independently derived from turn-aware states.
 * No routing logic is modified in order to make these tests pass.
 */
static int navit_routing_test_penalty_case(int case_id) {
    struct route_graph *graph = g_new0(struct route_graph, 1);
    struct vehicleprofile profile = { 0 };
    struct roadprofile road_profile = { 0 };
    struct route_graph_point *a, *b, *c, *d, *goal;
    struct route_graph_segment *ab, *bc, *bd, *cg, *dg;
    struct route_graph_segment_data data = { 0 };
    struct item road_item = { 0 };
    struct coord coords[] = { { 101, 101 }, { 102, 101 }, { 103, 101 },
                              { 104, 101 }, { 105, 101 } };
    int expected, observed, result;
    int initial_cost = -1;
    int initial_successor_is_d = 0;
    int final_successor_is_c = 0;

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
    goal = route_graph_add_point(graph, &coords[4]);

    road_item.type = type_street_0;
    data.item = &road_item;
    data.maxspeed = 36;

    /* Restricted incoming segment A->B. */
    road_item.id_lo = 101;
    data.flags = AF_SPEED_LIMIT | AF_THROUGH_TRAFFIC_LIMIT;
    data.len = 1;
    route_graph_add_segment(graph, a, b, &data);
    ab = a->start;

    /* Restricted continuation B->D. */
    road_item.id_lo = 102;
    data.flags = AF_SPEED_LIMIT | AF_THROUGH_TRAFFIC_LIMIT;
    data.len = (case_id == 4) ? 2 : 1;
    route_graph_add_segment(graph, b, d, &data);
    bd = b->start;

    /* Unrestricted, cheaper continuation B->C; inserted last to ensure
     * its presence is checked independently of the predecessor's cost.
     */
    road_item.id_lo = 103;
    data.flags = AF_SPEED_LIMIT;
    data.len = 1;
    route_graph_add_segment(graph, b, c, &data);
    bc = b->start;

    road_item.id_lo = 104;
    data.len = (case_id == 4) ? 1 : 2;
    route_graph_add_segment(graph, c, goal, &data);
    cg = c->start;

    /* Keep the D->Goal continuation restricted: otherwise Navit
     * correctly charges another exit penalty at D, invalidating
     * the oracle's purported penalty-free restricted alternative.
     */
    road_item.id_lo = 105;
    data.flags = AF_SPEED_LIMIT | AF_THROUGH_TRAFFIC_LIMIT;
    data.len = 1;
    route_graph_add_segment(graph, d, goal, &data);
    dg = d->start;

    goal->dst_val = 0;
    goal->rhs = 0;
    goal->el = fh_insertkey(graph->heap, 0, goal);
    route_graph_compute_shortest_path(graph, &profile, NULL);

    if (case_id == 4) {
        /* CORE-004 independent turn-aware oracle:
         * AB(1) + BD(2) + DG(1) = 4.
         * AB(1) + penalty(100) + BC(1) + CG(1) = 103.
         * A point-only choice at B instead favors the BC suffix.
         */
        expected = 4;
        observed = a->value;
        result = b->seg == bc && observed == expected
                 && a->value == a->rhs && !fh_min(graph->heap);
    } else if (case_id == 5) {
        /* INCR-006: initially B->D costs 2 versus B->C costs 3.
         * After simultaneous cost changes: B->C costs 2, B->D
         * costs 3. B's g stays at 2 while its chosen suffix changes.
         * The incoming restricted AB should instead use BD, with
         * complete route cost AB(1)+BD(1)+DG(2)=4.
         */
        initial_cost = a->value;
        initial_successor_is_d = b->seg == bd;
        cg->data.len = 1;
        dg->data.len = 2;
        route_graph_point_update(&profile, c, graph->heap);
        route_graph_point_update(&profile, d, graph->heap);
        route_graph_compute_shortest_path(graph, &profile, NULL);
        final_successor_is_c = b->seg == bc;
        expected = 4;
        observed = a->value;
        result = initial_cost == 3 && initial_successor_is_d && final_successor_is_c
                 && b->value == 2 && observed == expected
                 && a->value == a->rhs && !fh_min(graph->heap);
    } else {
        expected = -1;
        observed = -1;
        result = 0;
    }

    if (!result)
        fprintf(stderr,
                "RED production case %d: expected=%d actual=%d initial=%d "
                "initial_B_to_D=%d final_B_to_C=%d B_g=%d B_rhs=%d "
                "A_g=%d A_rhs=%d queue_empty=%d\n",
                case_id, expected, observed, initial_cost, initial_successor_is_d,
                final_successor_is_c, b->value, b->rhs, a->value, a->rhs,
                !fh_min(graph->heap));

    route_graph_free_segments(graph);
    route_graph_free_points(graph);
    fh_deleteheap(graph->heap);
    g_hash_table_destroy(profile.roadprofile_hash);
    g_free(graph);
    return result ? 0 : 1;
}
