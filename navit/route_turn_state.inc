/*
 * Experimental exact shortest-path search over directed incoming movements.
 *
 * This is linked into the actual route.c translation unit, rather than the
 * independent test oracle. It is not used by route_path_new() yet: the
 * existing destination-candidate, waypoint and route-map APIs require
 * separate integration and regression coverage.
 *
 * Each state records the road segment used to arrive at a graph point.
 * Transition penalties depend only on that incoming segment and on the
 * next movement, not on a mutable point->seg selected by another route.
 */
struct route_turn_state {
    struct route_graph_point *point;
    struct route_graph_segment *incoming;
    struct route_turn_state *previous;
    struct fibheap_el *element;
    int cost;
    int settled;
    int direction;
};

struct route_turn_path {
    GPtrArray *segments;
    GArray *directions;
    int cost;
};

static void route_turn_path_clear(struct route_turn_path *path) {
    if (path->segments)
        g_ptr_array_free(path->segments, TRUE);
    if (path->directions)
        g_array_free(path->directions, TRUE);
    path->segments = NULL;
    path->directions = NULL;
    path->cost = INT_MAX;
}

/* A state is uniquely identified by its incoming segment and direction.
 * All states use actual graph point pointers; restriction clones remain
 * distinct, including when their coordinates happen to be identical.
 */
static struct route_turn_state *route_turn_state_find(GHashTable *forward, GHashTable *reverse,
                                                       struct route_graph_segment *s, int direction) {
    return g_hash_table_lookup(direction > 0 ? forward : reverse, s);
}

static int route_turn_transition_cost(struct vehicleprofile *profile, struct route_graph_segment *previous,
                                      struct route_graph_segment *next, int direction) {
    int step = route_value_seg(profile, NULL, next, direction);
    int extra = 0;

    if (step == INT_MAX)
        return INT_MAX;
    if (previous) {
        if (item_is_equal(previous->data.item, next->data.item)) {
            if (!profile->turn_around_penalty2)
                return INT_MAX;
            extra = profile->turn_around_penalty2;
        }
        if (!route_through_traffic_allowed(profile, previous) && route_through_traffic_allowed(profile, next)) {
            if (profile->through_traffic_penalty < 0 || extra > INT_MAX - profile->through_traffic_penalty)
                return INT_MAX;
            extra += profile->through_traffic_penalty;
        }
    }
    if (step > INT_MAX - extra)
        return INT_MAX;
    return step + extra;
}

/*
 * Find a graph-vertex-to-vertex path. The graph profile is assumed to
 * have nonnegative traversal and transition costs. Uses a Fibonacci heap
 * with one entry per inconsistent candidate, and scans incident edges
 * only, giving the standard sparse Dijkstra exploration pattern.
 *
 * out is always initialized. The caller owns its path arrays on success.
 * This function never updates the production graph's value/rhs/seg/heap.
 */
static int route_turn_graph_path_from_arrival(struct route_graph *graph, struct vehicleprofile *profile,
                                              struct route_graph_point *start,
                                              struct route_graph_segment *incoming, int incoming_direction,
                                              struct route_graph_point *goal, struct route_turn_path *out) {
    GHashTable *forward = NULL, *reverse = NULL;
    GPtrArray *all = NULL;
    struct fibheap *heap = NULL;
    struct route_graph_segment *s;
    struct route_turn_state *state, *next, *winner = NULL;
    struct route_turn_state *initial;
    int direction, step, candidate;
    unsigned int i;

    out->segments = NULL;
    out->directions = NULL;
    out->cost = INT_MAX;
    if (!graph || !profile || !start || !goal || profile->through_traffic_penalty < 0
        || profile->turn_around_penalty2 < 0 || (incoming && incoming_direction != 1 && incoming_direction != -1))
        return 0;

    forward = g_hash_table_new(g_direct_hash, g_direct_equal);
    reverse = g_hash_table_new(g_direct_hash, g_direct_equal);
    all = g_ptr_array_new_with_free_func(g_free);
    heap = fh_makekeyheap();

    for (s = graph->route_segments; s; s = s->next) {
        if (s->data.item.type < route_item_first || s->data.item.type > route_item_last || s->start == s->end)
            continue;
        state = g_new0(struct route_turn_state, 1);
        state->point = s->end;
        state->incoming = s;
        state->direction = 1;
        state->cost = INT_MAX;
        g_hash_table_insert(forward, s, state);
        g_ptr_array_add(all, state);

        state = g_new0(struct route_turn_state, 1);
        state->point = s->start;
        state->incoming = s;
        state->direction = -1;
        state->cost = INT_MAX;
        g_hash_table_insert(reverse, s, state);
        g_ptr_array_add(all, state);
    }

    if (incoming) {
        initial = route_turn_state_find(forward, reverse, incoming, incoming_direction);
        if (!initial || initial->point != start)
            goto cleanup;
    } else {
        initial = g_new0(struct route_turn_state, 1);
        initial->point = start;
        g_ptr_array_add(all, initial);
    }
    initial->cost = 0;
    initial->element = fh_insertkey(heap, 0, initial);
    while ((state = fh_extractmin(heap))) {
        state->element = NULL;
        if (state->settled)
            continue;
        state->settled = 1;
        if (state->point == goal) {
            winner = state;
            break;
        }

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
        GPtrArray *backwards = g_ptr_array_new();
        out->segments = g_ptr_array_new();
        out->directions = g_array_new(FALSE, FALSE, sizeof(int));
        out->cost = winner->cost;
        for (state = winner; state && state != initial; state = state->previous)
            g_ptr_array_add(backwards, state);
        for (i = backwards->len; i > 0; i--) {
            state = g_ptr_array_index(backwards, i - 1);
            g_ptr_array_add(out->segments, state->incoming);
            g_array_append_val(out->directions, state->direction);
        }
        g_ptr_array_free(backwards, TRUE);
    }

cleanup:
    /* Discard pending heap elements before freeing their state objects. */
    fh_deleteheap(heap);
    g_hash_table_destroy(forward);
    g_hash_table_destroy(reverse);
    g_ptr_array_free(all, TRUE);
    return winner != NULL;
}

/* A route beginning at a junction has no incoming movement. A moving vehicle
 * already traversing a road can instead use route_turn_graph_path_from_arrival()
 * to preserve the incoming movement context, without charging it twice.
 */
static int route_turn_graph_path(struct route_graph *graph, struct vehicleprofile *profile,
                                 struct route_graph_point *start, struct route_graph_point *goal,
                                 struct route_turn_path *out) {
    return route_turn_graph_path_from_arrival(graph, profile, start, NULL, 0, goal, out);
}
