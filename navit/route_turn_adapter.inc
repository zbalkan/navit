/*
 * Convert exact incoming-movement plans into Navit's ordinary route_path
 * segments. The route graph itself owns all referenced road segments.
 * This adapter adds no geometric approximation; route_turn_select_plan()
 * invokes the graph search implemented by the turn planner.
 */
/* Select exact directed movements independently of path geometry.
 * A separate entry point permits differential testing of the same policy
 * that the production route adapter will consume.
 */
static int route_turn_select_plan(struct route_graph *graph, struct route_info *pos,
                                   struct route_info *dst, struct vehicleprofile *profile,
                                   struct route_turn_path *best, int *direct) {
    struct route_graph_segment *segment = NULL, *terminal;
    struct route_turn_path candidate = { 0 };
    int best_cost = INT_MAX;

    *direct = 0;
    if (!graph || !pos || !dst || !pos->street || !dst->street)
        return 0;

    while ((segment = route_graph_get_segment(graph, pos->street, segment))) {
        terminal = NULL;
        if (item_is_equal(pos->street->item, dst->street->item)) {
            while ((terminal = route_graph_get_segment(graph, dst->street, terminal))) {
                if (terminal != segment)
                    continue;
                if (route_turn_plan_direct_on_segment(profile, segment, pos->percent, dst->percent,
                                                       pos->street_direction, &candidate)
                    && candidate.cost < best_cost) {
                    route_turn_path_clear(best);
                    *best = candidate;
                    memset(&candidate, 0, sizeof(candidate));
                    best_cost = best->cost;
                    *direct = 1;
                }
                route_turn_path_clear(&candidate);
            }
        }
        if (route_turn_plan_from_segment(graph, profile, segment, pos->percent, pos->street_direction, &candidate)
            && candidate.cost < best_cost) {
            route_turn_path_clear(best);
            *best = candidate;
            memset(&candidate, 0, sizeof(candidate));
            best_cost = best->cost;
            *direct = 0;
        }
        route_turn_path_clear(&candidate);
    }
    return best_cost != INT_MAX;
}

static struct route_path *route_path_new_movement(struct route_graph *graph, struct route_path *oldpath,
                                                  struct route_info *pos, struct route_info *dst,
                                                  struct vehicleprofile *profile) {
    struct route_turn_path best = { 0 };
    struct route_path *result;
    unsigned int i;
    int direct = 0;

    if (!route_turn_select_plan(graph, pos, dst, profile, &best, &direct))
        return NULL;

    result = g_new0(struct route_path, 1);
    result->in_use = 1;
    result->updated = 1;
    result->path_hash = item_hash_new();
    if (pos->lenextra)
        route_path_add_line(result, &pos->c, &pos->lp, pos->lenextra);

    for (i = 0; i < best.len; ++i) {
        struct route_info *first = i == 0 ? pos : NULL;
        struct route_info *last = NULL;
        if (direct && i + 1 == best.len)
            last = dst;
        else if (i + 1 == best.len && best.terminal_segment == best.segments[i])
            last = dst;
        if (!route_path_add_item_from_graph(result, oldpath, best.segments[i], best.directions[i], first, last))
            result->updated = 0;
    }
    if (!direct && best.terminal_segment
        && (!best.len || best.terminal_segment != best.segments[best.len - 1])) {
        if (!route_path_add_item_from_graph(result, oldpath, best.terminal_segment,
                                             best.terminal_direction, NULL, dst))
            result->updated = 0;
    }
    if (dst->lenextra)
        route_path_add_line(result, &dst->lp, &dst->c, dst->lenextra);
    route_turn_path_clear(&best);
    return result;
}
