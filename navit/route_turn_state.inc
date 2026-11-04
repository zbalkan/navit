/*
 * Directed-movement shortest paths on Navit's production route graph.
 *
 * EXPERIMENTAL: not wired into route_path_new() or its destination
 * candidates. Keep algorithmic validation separate from integration.
 * This uses Navit's existing fibheap, GHashTable and g_new/g_free
 * abstractions, without GLib array types unavailable on some targets.
 */
struct route_turn_state {
    struct route_graph_point *point;
    struct route_graph_segment *incoming;
    struct route_turn_state *previous;
    struct route_turn_state *allocated_next;
    struct fibheap_el *element;
    int cost;
    int settled;
    int direction;
};

struct route_turn_path {
    struct route_graph_point *terminal;
    struct route_graph_segment **segments;
    int *directions;
    unsigned int len;
    int cost;
};

static void route_turn_path_clear(struct route_turn_path *path) {
    g_free(path->segments);
    g_free(path->directions);
    path->segments = NULL;
    path->directions = NULL;
    path->len = 0;
    path->terminal = NULL;
    path->cost = INT_MAX;
}

static struct route_turn_state *route_turn_state_find(GHashTable *forward, GHashTable *reverse,
                                                       struct route_graph_segment *s, int direction) {
    return g_hash_table_lookup(direction > 0 ? forward : reverse, s);
}

/* The outgoing movement's penalty is independent of its road length.
 * Destination partial-road candidates already carry their travel time
 * in dst_val, but must still pay their incoming-to-terminal turn cost.
 */
static int route_turn_transition_penalty(struct vehicleprofile *profile, struct route_graph_segment *previous,
                                         struct route_graph_segment *next) {
    int extra = 0;
    if (!previous)
        return 0;
    if (previous == next) {
        if (!profile->turn_around_penalty2)
            return INT_MAX;
        extra = profile->turn_around_penalty2;
    }
    if (!route_through_traffic_allowed(profile, previous) && route_through_traffic_allowed(profile, next)) {
        if (profile->through_traffic_penalty < 0 || extra > INT_MAX - profile->through_traffic_penalty)
            return INT_MAX;
        extra += profile->through_traffic_penalty;
    }
    return extra;
}

static int route_turn_transition_cost(struct vehicleprofile *profile, struct route_graph_segment *previous,
                                      struct route_graph_segment *next, int direction) {
    int step = route_value_seg(profile, NULL, next, direction);
    int extra = route_turn_transition_penalty(profile, previous, next);

    if (step == INT_MAX || extra == INT_MAX || step > INT_MAX - extra)
        return INT_MAX;
    return step + extra;
}

static int route_turn_terminal_cost(struct vehicleprofile *profile, struct route_turn_state *state) {
    struct route_graph_segment *segment = state->point->dst_seg;
    int direction, extra, remaining = state->point->dst_val;

    if (remaining == INT_MAX || remaining < 0 || !segment)
        return remaining;
    if (segment->start == state->point)
        direction = 1;
    else if (segment->end == state->point)
        direction = -1;
    else
        return INT_MAX;
    /* A destination candidate's partial segment must remain traversable
     * in the departure direction, even after a traffic revision.
     */
    if (route_value_seg(profile, NULL, segment, direction) == INT_MAX)
        return INT_MAX;
    extra = route_turn_transition_penalty(profile, state->incoming, segment);
    if (extra == INT_MAX || remaining > INT_MAX - extra)
        return INT_MAX;
    return remaining + extra;
}

static int route_turn_graph_path_from_arrival(struct route_graph *graph, struct vehicleprofile *profile,
                                              struct route_graph_point *start,
                                              struct route_graph_segment *incoming, int incoming_direction,
                                              struct route_graph_point *goal, struct route_turn_path *out) {
    GHashTable *forward, *reverse;
    struct route_turn_state *allocated = NULL, *initial, *state, *next, *winner = NULL;
    struct fibheap *heap;
    struct route_graph_segment *s;
    int direction, step, candidate, terminal_cost, best_terminal = INT_MAX;
    unsigned int i, length = 0;

    out->segments = NULL;
    out->directions = NULL;
    out->len = 0;
    out->terminal = NULL;
    out->cost = INT_MAX;
    if (!graph || !profile || !start || profile->through_traffic_penalty < 0
        || profile->turn_around_penalty2 < 0 || (incoming && incoming_direction != 1 && incoming_direction != -1))
        return 0;

    forward = g_hash_table_new(g_direct_hash, g_direct_equal);
    reverse = g_hash_table_new(g_direct_hash, g_direct_equal);
    heap = fh_makekeyheap();

    for (s = graph->route_segments; s; s = s->next) {
        if (s->data.item.type < route_item_first || s->data.item.type > route_item_last || s->start == s->end)
            continue;
        state = g_new0(struct route_turn_state, 1);
        state->point = s->end;
        state->incoming = s;
        state->direction = 1;
        state->cost = INT_MAX;
        state->allocated_next = allocated;
        allocated = state;
        g_hash_table_insert(forward, s, state);

        state = g_new0(struct route_turn_state, 1);
        state->point = s->start;
        state->incoming = s;
        state->direction = -1;
        state->cost = INT_MAX;
        state->allocated_next = allocated;
        allocated = state;
        g_hash_table_insert(reverse, s, state);
    }

    if (incoming) {
        initial = route_turn_state_find(forward, reverse, incoming, incoming_direction);
        if (!initial || initial->point != start)
            goto cleanup;
    } else {
        initial = g_new0(struct route_turn_state, 1);
        initial->point = start;
        initial->allocated_next = allocated;
        allocated = initial;
    }

    initial->cost = 0;
    initial->element = fh_insertkey(heap, 0, initial);

    while ((state = fh_extractmin(heap))) {
        state->element = NULL;
        if (state->settled)
            continue;
        state->settled = 1;
        /* Dijkstra is nonnegative. No unsettled point can improve a
         * terminal whose total distance is already <= this distance.
         */
        if (state->cost >= best_terminal)
            break;
        terminal_cost = goal ? (state->point == goal ? 0 : INT_MAX) : route_turn_terminal_cost(profile, state);
        if (terminal_cost != INT_MAX && terminal_cost >= 0
            && state->cost <= INT_MAX - terminal_cost
            && state->cost + terminal_cost < best_terminal) {
            winner = state;
            best_terminal = state->cost + terminal_cost;
        }
        if (goal && winner)
            break;

        for (direction = 1; direction >= -1; direction -= 2) {
            s = direction > 0 ? state->point->start : state->point->end;
            while (s) {
                struct route_graph_segment *current = s;
                s = direction > 0 ? s->start_next : s->end_next;
                next = route_turn_state_find(forward, reverse, current, direction);
                if (!next || next->settled)
                    continue;
                step = route_turn_transition_cost(profile, state->incoming, current, direction);
                if (step == INT_MAX || state->cost > INT_MAX - step)
                    continue;
                candidate = state->cost + step;
                if (candidate >= next->cost)
                    continue;
                next->cost = candidate;
                next->previous = state;
                if (next->element) {
                    fh_delete(heap, next->element);
                    next->element = NULL;
                }
                next->element = fh_insertkey(heap, candidate, next);
            }
        }
    }

    if (winner) {
        out->cost = best_terminal;
        out->terminal = winner->point;
        for (state = winner; state && state != initial; state = state->previous)
            length++;
        out->len = length;
        if (length) {
            out->segments = g_new0(struct route_graph_segment *, length);
            out->directions = g_new0(int, length);
        }
        i = length;
        for (state = winner; state && state != initial; state = state->previous) {
            i--;
            out->segments[i] = state->incoming;
            out->directions[i] = state->direction;
        }
    }

cleanup:
    fh_deleteheap(heap);
    g_hash_table_destroy(forward);
    g_hash_table_destroy(reverse);
    while (allocated) {
        state = allocated->allocated_next;
        g_free(allocated);
        allocated = state;
    }
    return winner != NULL;
}

static int route_turn_graph_path(struct route_graph *graph, struct vehicleprofile *profile,
                                 struct route_graph_point *start, struct route_graph_point *goal,
                                 struct route_turn_path *out) {
    return route_turn_graph_path_from_arrival(graph, profile, start, NULL, 0, goal, out);
}

/* Destination metadata from route_graph_init() may identify multiple
 * graph points with different remaining partial-road costs. This
 * computes the cheapest junction-to-terminal cost and returns the
 * chosen terminal point; route extraction still has to append the
 * terminal's dst_seg partial road.
 */
static int route_turn_graph_path_to_terminals(struct route_graph *graph, struct vehicleprofile *profile,
                                              struct route_graph_point *start, struct route_turn_path *out) {
    return route_turn_graph_path_from_arrival(graph, profile, start, NULL, 0, NULL, out);
}
