/* Focused production-semantics tests for self-loops and exact reversals. */
static int navit_routing_test_arrival_semantics(int case_id) {
    struct route_graph *graph = g_new0(struct route_graph, 1);
    struct vehicleprofile profile = { 0 };
    struct roadprofile road_profile = { 0 };
    struct route_graph_point *a, *b, *c, *d;
    struct route_graph_segment *first, *other;
    struct route_graph_segment_data data = { 0 };
    struct item item = { 0 };
    struct coord coords[] = { { 501, 501 }, { 502, 501 }, { 503, 501 }, { 504, 501 } };
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
        route_graph_add_segment(graph, c, b, &data);
        other = c->start;
        b->seg = other;
        actual = route_arrival_step(&profile, first, 1, b, 0);
        if (result) {
            expected = 51;
            result = actual == expected;
        }
    } else if (case_id == 37 || case_id == 38) {
        /* Splitting a road item at a junction must permit continuing in
         * the same direction without any turnaround penalty. */
        route_graph_add_segment(graph, a, b, &data);
        first = a->start;
        route_graph_add_segment(graph, b, c, &data);
        other = b->start;
        b->seg = other;
        b->value = 0;
        if (case_id == 38)
            profile.turn_around_penalty2 = 0;
        actual = route_arrival_step(&profile, first, 1, b, 0);
        expected = 1;
        result = actual == expected;
    } else if (case_id == 39) {
        /* The same-item continuation must also survive full graph flooding. */
        route_graph_add_segment(graph, a, b, &data);
        route_graph_add_segment(graph, b, c, &data);
        route_graph_seed_destination(graph, &profile, c, NULL, 0, 0);
        route_graph_compute_shortest_path(graph, &profile, NULL);
        actual = a->value;
        expected = 2;
        result = actual == expected;
    } else if (case_id == 46) {
        /* Cost accumulation must saturate at the unreachable sentinel. */
        actual = route_value_add(INT_MAX - 5, 10);
        expected = INT_MAX;
        result = actual == expected;
        result = result && route_value_add(INT_MAX, 1) == INT_MAX;
        result = result && route_value_add(2, 3) == 5;
    } else if (case_id == 41) {
        struct attr invalid = { 0 };
        struct attr queried = { 0 };
        invalid.type = attr_through_traffic_penalty;
        invalid.u.num = -1;
        actual = vehicleprofile_set_attr(&profile, &invalid);
        expected = 0;
        result = actual == expected;
        result = result && profile.through_traffic_penalty == 100;
        result = result && !vehicleprofile_get_attr(&profile, attr_through_traffic_penalty, &queried, NULL);
    } else if (case_id == 40) {
        /* Two open-road arrivals at B require different continuations.
         * Reverse-direction same-item B->C is cheap for other arrivals,
         * but forbidden following A->B; A must take B->D->C instead.
         * Arrival class alone cannot encode this transition constraint.
         */
        struct route_turn_path oracle = { 0 }, selected = { 0 };
        struct street_data *start_street, *destination_street;
        struct route_info position = { 0 }, destination = { 0 };
        struct route_graph_segment *ab, *bd, *dc;
        int direct = 0;
        d = route_graph_add_point(graph, &coords[3]);
        profile.turn_around_penalty2 = 0;
        route_graph_add_segment(graph, a, b, &data);
        ab = a->start;
        route_graph_add_segment(graph, c, b, &data);
        item.id_lo = 102;
        data.len = 2;
        route_graph_add_segment(graph, b, d, &data);
        bd = b->start;
        item.id_lo = 103;
        route_graph_add_segment(graph, d, c, &data);
        dc = d->start;
        route_graph_seed_destination(graph, &profile, c, NULL, 0, 0);
        route_graph_compute_shortest_path(graph, &profile, NULL);

        /* Match only the road fragment beginning at A, not the other
         * same-item segment beginning at C. The production adapter
         * uses this exact selector before geometry construction.
         */
        start_street = g_malloc0(sizeof(*start_street) + 2 * sizeof(struct coord));
        destination_street = g_malloc0(sizeof(*destination_street) + 2 * sizeof(struct coord));
        start_street->item = ab->data.item;
        start_street->count = 2;
        start_street->c[0] = a->c;
        start_street->c[1] = b->c;
        destination_street->item = dc->data.item;
        destination_street->count = 2;
        destination_street->c[0] = d->c;
        destination_street->c[1] = c->c;
        position.street = start_street;
        position.street_direction = 1;
        position.percent = 0;
        destination.street = destination_street;
        destination.percent = 100;
        expected = 5;
        result = route_turn_graph_path(graph, &profile, a, c, &oracle);
        result = result && oracle.cost == expected;
        result = result && route_turn_select_plan(graph, &position, &destination, &profile, &selected, &direct);
        actual = selected.cost;
        result = result && !direct && actual == expected && selected.len == 3;
        if (selected.len == 3)
            result = result && selected.segments[0] == ab && selected.segments[1] == bd
                     && selected.segments[2] == dc
                     && selected.directions[0] == 1 && selected.directions[1] == 1
                     && selected.directions[2] == 1;
        route_turn_path_clear(&oracle);
        route_turn_path_clear(&selected);
        g_free(start_street);
        g_free(destination_street);
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
