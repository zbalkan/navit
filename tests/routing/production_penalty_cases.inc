/* CORE-004 and INCR-006: production shortest-path tests using actual
 * route_value_seg(), route_graph_point_update(), and LPA* propagation.
 * These fixtures express costs in tenths of a second at 36 km/h.
 *
 * Their oracle costs are independently derived from turn-aware states.
 * No routing logic is modified in order to make these tests pass.
 */
static int navit_routing_test_stateful_cost(struct route_graph *graph, struct vehicleprofile *profile,
                                            struct route_graph_point *start, struct route_graph_point *goal);

static int navit_routing_test_penalty_case(int case_id) {
    struct route_graph *graph = g_new0(struct route_graph, 1);
    struct vehicleprofile profile = { 0 };
    struct roadprofile road_profile = { 0 };
    struct route_graph_point *a, *b, *c, *d, *goal, *x = NULL;
    struct route_graph_segment *ab, *bc, *bd, *cg, *dg, *xb = NULL;
    struct route_graph_segment_data data = { 0 };
    struct item road_item = { 0 };
    struct coord coords[] = { { 101, 101 }, { 102, 101 }, { 103, 101 },
                              { 104, 101 }, { 105, 101 }, { 106, 101 } };
    int expected, observed, result;
    int initial_cost = -1;
    int initial_successor_is_d = 0;
    int final_successor_is_c = 0;
    int x_cost = -1;
    int incremental_cost = INT_MAX;
    int scratch_cost = INT_MAX;
    int independent_cost = INT_MAX;
    int fixture_case = case_id == 29 || case_id == 32 ? 5
                       : (case_id == 31 ? 4 : (case_id == 33 ? 6
                       : (case_id >= 10 ? case_id - 6 : (case_id > 6 ? case_id - 3 : case_id))));
    struct route_turn_path path = { 0 }, second = { 0 };

    graph->heap = fh_makekeyheap();
    profile.roadprofile_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
    /* Include the default production penalty, rather than testing only P=100. */
    profile.through_traffic_penalty = case_id >= 31 && case_id <= 33 ? 9000 : 100;
    road_profile.speed = 36;
    road_profile.maxspeed = 36;
    g_hash_table_insert(profile.roadprofile_hash, (gpointer)(long)type_street_0, &road_profile);

    a = route_graph_add_point(graph, &coords[0]);
    b = route_graph_add_point(graph, &coords[1]);
    c = route_graph_add_point(graph, &coords[2]);
    d = route_graph_add_point(graph, &coords[3]);
    goal = route_graph_add_point(graph, &coords[4]);
    if (fixture_case == 6)
        x = route_graph_add_point(graph, &coords[5]);

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
    data.len = (fixture_case == 4 || fixture_case == 6) ? 2 : 1;
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
    data.len = (fixture_case == 4 || fixture_case == 6) ? 1 : 2;
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

    if (fixture_case == 6) {
        /* A second unrestricted arrival at B; optimal continuation is
         * BC rather than BD. A single B->seg cannot encode both.
         */
        road_item.id_lo = 106;
        data.flags = AF_SPEED_LIMIT;
        data.len = 1;
        route_graph_add_segment(graph, x, b, &data);
        xb = x->start;
    }

    route_graph_seed_destination(graph, &profile, goal, NULL, 0, 0);
    route_graph_compute_shortest_path(graph, &profile, NULL);

    if (fixture_case == 4) {
        /* CORE-004 independent turn-aware oracle:
         * AB(1) + BD(2) + DG(1) = 4.
         * AB(1) + penalty(100) + BC(1) + CG(1) = 103.
         * A point-only choice at B instead favors the BC suffix.
         */
        expected = 4;
        independent_cost = navit_routing_test_stateful_cost(graph, &profile, a, goal);
        observed = case_id == 10 ? (route_turn_graph_path(graph, &profile, a, goal, &path) ? path.cost : INT_MAX)
                   : (case_id == 7 ? navit_routing_test_stateful_cost(graph, &profile, a, goal) : a->value);
        result = observed == expected && independent_cost == expected
                 && (case_id == 7 || case_id == 10 || b->seg == bc)
                 && a->value == a->rhs && !fh_min(graph->heap);
        if (case_id == 4 && profile.through_traffic_penalty > 0)
            result = result && a->seg == ab && b->restricted_seg == bd && d->restricted_seg == dg;
        if (case_id == 10)
            result = result && path.segments && path.len == 3
                     && path.segments[0] == ab
                     && path.segments[1] == bd
                     && path.segments[2] == dg
                     && path.directions[0] == 1
                     && path.directions[1] == 1
                     && path.directions[2] == 1;
    } else if (fixture_case == 6) {
        expected = 4;
        independent_cost = navit_routing_test_stateful_cost(graph, &profile, a, goal);
        observed = case_id == 12 ? (route_turn_graph_path(graph, &profile, a, goal, &path) ? path.cost : INT_MAX)
                   : (case_id == 9 ? navit_routing_test_stateful_cost(graph, &profile, a, goal) : a->value);
        x_cost = case_id == 12 ? (route_turn_graph_path(graph, &profile, x, goal, &second) ? second.cost : INT_MAX)
                 : (case_id == 9 ? navit_routing_test_stateful_cost(graph, &profile, x, goal) : x->value);
        result = observed == expected && independent_cost == expected && x_cost == 3
                 && a->value == a->rhs && x->value == x->rhs
                 && !fh_min(graph->heap);
        if (case_id == 6 && profile.through_traffic_penalty > 0)
            result = result && a->seg == ab && x->seg == xb
                     && b->restricted_seg == bd && b->seg == bc
                     && d->restricted_seg == dg;
        if (case_id == 12)
            result = result && path.segments && second.segments
                     && path.len == 3 && second.len == 3
                     && path.segments[0] == ab
                     && path.segments[1] == bd
                     && path.segments[2] == dg
                     && second.segments[0] == xb
                     && second.segments[1] == bc
                     && second.segments[2] == cg;
        if (case_id == 12) {
            route_turn_path_clear(&path);
            route_turn_path_clear(&second);
            result = result
                     && route_turn_graph_path_from_arrival(graph, &profile, b, ab, 1, goal, &path)
                     && route_turn_graph_path_from_arrival(graph, &profile, b, xb, 1, goal, &second);
            if (result)
                result = path.cost == 3 && second.cost == 2
                         && path.len == 2 && second.len == 2
                         && path.segments[0] == bd
                         && path.segments[1] == dg
                         && second.segments[0] == bc
                         && second.segments[1] == cg;
        }
    } else if (fixture_case == 5) {
        /* INCR-006: initially B->D costs 2 versus B->C costs 3.
         * After simultaneous cost changes: B->C costs 2, B->D
         * costs 3. B's g stays at 2 while its chosen suffix changes.
         * The incoming restricted AB should instead use BD, with
         * complete route cost AB(1)+BD(1)+DG(2)=4.
         */
        initial_cost = case_id == 11 ? (route_turn_graph_path(graph, &profile, a, goal, &path) ? path.cost : INT_MAX)
                       : (case_id == 8 ? navit_routing_test_stateful_cost(graph, &profile, a, goal) : a->value);
        if (case_id == 11)
            route_turn_path_clear(&path);
        initial_successor_is_d = b->seg == bd;
        cg->data.len = 1;
        dg->data.len = 2;
        route_graph_point_update(&profile, c, graph->heap);
        route_graph_point_update(&profile, d, graph->heap);
        route_graph_compute_shortest_path(graph, &profile, NULL);
        final_successor_is_c = b->seg == bc;
        expected = 4;
        independent_cost = navit_routing_test_stateful_cost(graph, &profile, a, goal);
        observed = case_id == 11 ? (route_turn_graph_path(graph, &profile, a, goal, &path) ? path.cost : INT_MAX)
                   : (case_id == 8 ? navit_routing_test_stateful_cost(graph, &profile, a, goal) : a->value);
        result = initial_cost == 3 && initial_successor_is_d && final_successor_is_c
                 && b->value == 2 && observed == expected && independent_cost == expected
                 && a->value == a->rhs && !fh_min(graph->heap);
        if (case_id == 29) {
            /* INCR-006a: isolate incremental consistency from oracle correctness.
             * Re-flood the identical final graph without reusing prior labels. */
            incremental_cost = a->value;
            route_graph_reset(graph);
            route_graph_seed_destination(graph, &profile, goal, NULL, 0, 0);
            route_graph_compute_shortest_path(graph, &profile, NULL);
            scratch_cost = a->value;
            result = initial_cost == 3 && initial_successor_is_d && final_successor_is_c
                     && incremental_cost == scratch_cost && a->value == a->rhs
                     && !fh_min(graph->heap);
            if (!result)
                fprintf(stderr, "INCR-006a: incremental=%d scratch=%d\n",
                        incremental_cost, scratch_cost);
        }
        if (case_id == 5 && profile.through_traffic_penalty > 0)
            result = result && b->restricted_seg == bd && b->seg == bc;
        if (case_id == 11)
            result = result && path.segments && path.len == 3
                     && path.segments[0] == ab
                     && path.segments[1] == bd
                     && path.segments[2] == dg;
    } else {
        expected = -1;
        observed = -1;
        result = 0;
    }

    if (!result)
        fprintf(stderr,
                "RED production case %d (P=%d): expected=%d actual=%d initial=%d "
                "initial_B_to_D=%d final_B_to_C=%d B_g=%d B_rhs=%d "
                "A_g=%d A_rhs=%d X_g=%d oracle=%d queue_empty=%d\n",
                case_id, profile.through_traffic_penalty, expected, observed, initial_cost, initial_successor_is_d,
                final_successor_is_c, b->value, b->rhs, a->value, a->rhs,
                x_cost, independent_cost, !fh_min(graph->heap));

    route_turn_path_clear(&path);
    route_turn_path_clear(&second);
    route_graph_free_segments(graph);
    route_graph_free_points(graph);
    fh_deleteheap(graph->heap);
    g_hash_table_destroy(profile.roadprofile_hash);
    g_free(graph);
    return result ? 0 : 1;
}
