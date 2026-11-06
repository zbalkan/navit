/* Movement-state semantics across real route_graph_segment objects.
 * Keep these tests separate from the cost-only Red fixture: they establish
 * direction handling and continuity across segmented representations.
 */
static int navit_routing_test_movement_semantics(int case_id) {
    struct route_graph *graph = g_new0(struct route_graph, 1);
    struct vehicleprofile profile = { 0 };
    struct roadprofile road_profile = { 0 };
    struct route_graph_point *a, *b, *c;
    struct route_graph_segment *ab, *bc = NULL;
    struct route_graph_segment_data data = { 0 };
    struct item road_item = { 0 };
    struct route_turn_path path = { 0 };
    struct coord coords[] = { { 21001, 21001 }, { 21002, 21001 }, { 21003, 21001 } };
    int result = 0;
    int observed = INT_MAX;

    graph->heap = fh_makekeyheap();
    profile.roadprofile_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
    road_profile.speed = 36;
    road_profile.maxspeed = 36;
    g_hash_table_insert(profile.roadprofile_hash, (gpointer)(long)type_street_0, &road_profile);
    profile.flags_forward_mask = AF_ONEWAYREV;
    profile.flags_reverse_mask = AF_ONEWAY;

    a = route_graph_add_point(graph, &coords[0]);
    b = route_graph_add_point(graph, &coords[1]);
    c = route_graph_add_point(graph, &coords[2]);
    road_item.type = type_street_0;
    road_item.id_lo = 500;
    data.item = &road_item;
    data.maxspeed = 36;
    data.flags = AF_SPEED_LIMIT;
    data.len = 1;

    if (case_id == 14)
        data.flags |= AF_ONEWAY;
    route_graph_add_segment(graph, a, b, &data);
    ab = a->start;

    if (case_id == 13 || case_id == 16 || case_id == 17 || case_id == 18) {
        /* Two consecutive graph segments represent the same OSM road.
         * Continuing straight must be legal, even if item IDs match.
         */
        route_graph_add_segment(graph, b, c, &data);
        bc = b->start;
        if (case_id == 18) {
            /* Run full state search after each real traffic distortion
             * event. This is the correctness fallback before an
             * incremental state-repair implementation exists.
             */
            road_item.id_lo = 600;
            data.len = 5;
            route_graph_add_segment(graph, a, c, &data);
            result = route_turn_graph_path(graph, &profile, a, c, &path);
            observed = path.cost;
            result = result && observed == 2 && path.len == 2;
            route_turn_path_clear(&path);

            route_graph_set_traffic_distortion(graph, bc, 10);
            result = result && route_turn_graph_path(graph, &profile, a, c, &path);
            observed = path.cost;
            result = result && observed == 5 && path.len == 1;
            route_turn_path_clear(&path);

            route_graph_set_traffic_distortion(graph, bc, 0);
            result = result && route_turn_graph_path(graph, &profile, a, c, &path);
            observed = path.cost;
            result = result && observed == 2 && path.len == 2;
        } else if (case_id == 13) {
            result = route_turn_graph_path(graph, &profile, a, c, &path);
            observed = path.cost;
            result = result && observed == 2 && path.len == 2
                     && path.segments[0] == ab && path.segments[1] == bc;
        } else {
            /* Terminal B is nearby with an expensive remainder. Terminal
             * C is farther but cheaper to finish. The cost must include
             * each terminal's remaining partial-segment travel.
             */
            b->dst_val = 5;
            b->restricted_dst_val = 5;
            b->restricted_dst_seg = b->dst_seg;
            b->restricted_dst_fraction = -1;
            c->dst_val = 1;
            c->restricted_dst_val = 1;
            c->restricted_dst_seg = c->dst_seg;
            c->restricted_dst_fraction = -1;
            result = route_turn_graph_path_to_terminals(graph, &profile, a, &path);
            observed = path.cost;
            result = result && observed == 3 && path.terminal == c
                     && path.len == 2 && path.segments[1] == bc;
            if (case_id == 17 && result) {
                /* A destination-remainder revision changes the selected
                 * terminal without requiring the road graph to change.
                 */
                c->dst_val = 10;
                c->restricted_dst_val = 10;
                route_turn_path_clear(&path);
                result = route_turn_graph_path_to_terminals(graph, &profile, a, &path);
                observed = path.cost;
                result = result && observed == 6 && path.terminal == b
                         && path.len == 1 && path.segments[0] == ab;
            }
        }
    } else if (case_id == 50) {
        /* A different segment of the same OSM road, traversed in the
         * opposite direction, pays the configured turnaround penalty.
         * Compare it with an independently identified forward road.
         */
        struct item alternate_item = { 0 };
        struct route_graph_segment *reverse_piece, *alternate;
        profile.turn_around_penalty2 = 12;
        data.len = 3;
        route_graph_add_segment(graph, c, b, &data);
        reverse_piece = c->start;
        alternate_item.type = type_street_0;
        alternate_item.id_lo = 501;
        data.item = &alternate_item;
        data.len = 10;
        route_graph_add_segment(graph, b, c, &data);
        alternate = b->start;

        result = route_turn_graph_path_from_arrival(graph, &profile, b, ab, 1, c, &path);
        observed = path.cost;
        result = result && observed == 10 && path.len == 1
                 && path.segments[0] == alternate && path.directions[0] == 1;
        route_turn_path_clear(&path);

        profile.turn_around_penalty2 = 2;
        result = result && route_turn_graph_path_from_arrival(graph, &profile, b, ab, 1, c, &path);
        observed = path.cost;
        result = result && observed == 5 && path.len == 1
                 && path.segments[0] == reverse_piece && path.directions[0] == -1;
    } else if (case_id == 51) {
        /* Recompute the terminal partial-road cost from the fraction
         * and include the restricted-to-open transition penalty.
         */
        struct item destination_item = { 0 };
        struct route_graph_segment *terminal_road;
        ab->data.flags |= AF_THROUGH_TRAFFIC_LIMIT;
        profile.through_traffic_penalty = 100;
        destination_item.type = type_street_0;
        destination_item.id_lo = 502;
        data.item = &destination_item;
        data.len = 10;
        route_graph_add_segment(graph, b, c, &data);
        terminal_road = b->start;
        b->dst_seg = terminal_road;
        b->dst_fraction = 50;
        /* Deliberately stale: 50% of the 10-tick road is 5, not 2. */
        b->dst_val = 2;
        b->restricted_dst_seg = terminal_road;
        b->restricted_dst_fraction = 50;
        b->restricted_dst_val = 2;

        result = route_turn_graph_path_to_terminals(graph, &profile, a, &path);
        observed = path.cost;
        result = result && observed == 106 && path.len == 1
                 && path.terminal == b && path.terminal_segment == terminal_road
                 && path.terminal_direction == 1 && path.segments[0] == ab;

        route_turn_path_clear(&path);
        profile.through_traffic_penalty = 0;
        result = result && route_turn_graph_path_to_terminals(graph, &profile, a, &path);
        observed = path.cost;
        result = result && observed == 6 && path.len == 1
                 && path.terminal == b && path.terminal_segment == terminal_road;
    } else if (case_id == 14) {
        /* Forward one-way is valid; the opposite direction is not. */
        result = route_turn_graph_path(graph, &profile, a, b, &path);
        observed = path.cost;
        result = result && observed == 1 && path.len == 1 && path.segments[0] == ab;
        route_turn_path_clear(&path);
        result = result && !route_turn_graph_path(graph, &profile, b, a, &path)
                 && path.cost == INT_MAX;
    } else if (case_id == 48) {
        /* Exact segment reversal stays forbidden even when turnaround
         * penalties are enabled; a different road returning to A is legal.
         * Arrival-class-only state cannot distinguish these choices.
         */
        struct item return_item = { 0 };
        struct route_graph_segment *return_road;
        return_item.type = type_street_0;
        return_item.id_lo = 501;
        data.item = &return_item;
        data.len = 3;
        profile.turn_around_penalty2 = 50;
        route_graph_add_segment(graph, b, a, &data);
        return_road = b->start;
        result = route_turn_graph_path_from_arrival(graph, &profile, b, ab, 1, a, &path);
        observed = path.cost;
        result = result && observed == 3 && path.len == 1
                 && path.segments[0] == return_road && path.directions[0] == 1;
    } else if (case_id == 15) {
        /* An immediate U-turn on the incoming segment is forbidden
         * when the vehicle profile has no turnaround allowance.
         */
        result = !route_turn_graph_path_from_arrival(graph, &profile, b, ab, 1, a, &path);
        observed = path.cost;
        result = result && observed == INT_MAX;
    }
    if (!result)
        fprintf(stderr, "Movement-state semantic case %d failed: cost=%d\n", case_id, observed);
    route_turn_path_clear(&path);
    route_graph_free_segments(graph);
    route_graph_free_points(graph);
    fh_deleteheap(graph->heap);
    g_hash_table_destroy(profile.roadprofile_hash);
    g_free(graph);
    return result ? 0 : 1;
}
