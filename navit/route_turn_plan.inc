/*
 * Candidate selection for a position matched to the interior of a road.
 * The first segment is partially traversed. Subsequent movements retain
 * the direction and identity of that segment as their arrival context.
 *
 * This produces a segment/direction plan, not a route_path with geometry.
 * In particular, destination-on-start-segment and waypoint matching
 * require additional cases before it may replace route_path_new().
 */
static int route_turn_plan_from_segment(struct route_graph *graph, struct vehicleprofile *profile,
                                        struct route_graph_segment *start_segment, int percent,
                                        int street_direction, struct route_turn_path *out) {
    int dir, full, fraction, partial, penalty, total, best = INT_MAX;
    struct route_graph_point *endpoint;
    struct route_turn_path suffix = { 0 };
    unsigned int i;

    out->segments = NULL;
    out->directions = NULL;
    out->terminal = NULL;
    out->len = 0;
    out->cost = INT_MAX;
    if (!graph || !profile || !start_segment || percent < 0 || percent > 100
        || (street_direction != -1 && street_direction != 0 && street_direction != 1)
        || profile->turn_around_penalty < 0)
        return 0;

    for (dir = 1; dir >= -1; dir -= 2) {
        endpoint = dir > 0 ? start_segment->end : start_segment->start;
        full = route_value_seg(profile, NULL, start_segment, dir * 2);
        if (full == INT_MAX || full < 0)
            continue;
        fraction = dir > 0 ? 100 - percent : percent;
        partial = (full / 100) * fraction + ((full % 100) * fraction) / 100;
        penalty = street_direction && street_direction != dir ? profile->turn_around_penalty : 0;
        if (partial > INT_MAX - penalty)
            continue;
        if (!route_turn_graph_path_from_arrival(graph, profile, endpoint, start_segment, dir, NULL, &suffix))
            continue;
        if (suffix.cost > INT_MAX - partial - penalty) {
            route_turn_path_clear(&suffix);
            continue;
        }
        total = partial + penalty + suffix.cost;
        if (total < best) {
            route_turn_path_clear(out);
            out->len = suffix.len + 1;
            out->segments = g_new0(struct route_graph_segment *, out->len);
            out->directions = g_new0(int, out->len);
            out->segments[0] = start_segment;
            out->directions[0] = dir;
            for (i = 0; i < suffix.len; ++i) {
                out->segments[i + 1] = suffix.segments[i];
                out->directions[i + 1] = suffix.directions[i];
            }
            out->terminal = suffix.terminal;
            out->cost = total;
            best = total;
        }
        route_turn_path_clear(&suffix);
    }
    return best != INT_MAX;
}
