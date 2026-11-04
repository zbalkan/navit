/* Test-only reference implementation over actual Navit graph segments.
 * This is a correctness prototype, not a replacement for the production
 * LPA* data structures or routing-path extraction.
 *
 * A search state records the incoming directed road movement, so the
 * transition penalty is independent of the previously selected suffix.
 * Deliberately use quadratic Dijkstra to keep reference behavior simple.
 */
struct routing_test_state {
    struct route_graph_point *point;
    struct route_graph_segment *incoming;
    int distance;
    int settled;
};

static int navit_routing_test_stateful_cost(struct route_graph *graph, struct vehicleprofile *profile,
                                            struct route_graph_point *start, struct route_graph_point *goal) {
    struct routing_test_state *states;
    struct route_graph_segment *s;
    struct route_graph_point *to;
    int count = 1;
    int i, j, choice, best, dir, step, penalty, candidate, target;
    int answer = INT_MAX;

    for (s = graph->route_segments; s; s = s->next)
        count += 2;

    states = g_new0(struct routing_test_state, count);
    states[0].point = start;
    states[0].distance = 0;

    i = 1;
    for (s = graph->route_segments; s; s = s->next) {
        states[i].point = s->end;
        states[i].incoming = s;
        states[i].distance = INT_MAX;
        i++;
        states[i].point = s->start;
        states[i].incoming = s;
        states[i].distance = INT_MAX;
        i++;
    }

    for (;;) {
        best = INT_MAX;
        choice = -1;
        for (i = 0; i < count; i++) {
            if (!states[i].settled && states[i].distance < best) {
                best = states[i].distance;
                choice = i;
            }
        }
        if (choice < 0)
            break;
        if (states[choice].point == goal) {
            answer = best;
            break;
        }
        states[choice].settled = 1;
        for (s = graph->route_segments; s; s = s->next) {
            if (s->start == states[choice].point) {
                to = s->end;
                dir = 1;
                target = -1;
                for (j = 1; j < count; j++)
                    if (states[j].point == to && states[j].incoming == s) {
                        target = j;
                        break;
                    }
            } else if (s->end == states[choice].point) {
                to = s->start;
                dir = -1;
                target = -1;
                for (j = 1; j < count; j++)
                    if (states[j].point == to && states[j].incoming == s) {
                        target = j;
                        break;
                    }
            } else {
                continue;
            }
            if (to == states[choice].point || target < 0 || states[target].settled)
                continue;

            step = route_value_seg(profile, NULL, s, dir);
            if (step == INT_MAX)
                continue;
            penalty = 0;
            if (states[choice].incoming) {
                if (item_is_equal(states[choice].incoming->data.item, s->data.item)) {
                    if (!profile->turn_around_penalty2)
                        continue;
                    penalty = profile->turn_around_penalty2;
                }
                if (!route_through_traffic_allowed(profile, states[choice].incoming)
                    && route_through_traffic_allowed(profile, s))
                    penalty += profile->through_traffic_penalty;
            }
            if (best > INT_MAX - step || penalty > INT_MAX - best - step)
                continue;
            candidate = best + step + penalty;
            if (candidate < states[target].distance)
                states[target].distance = candidate;
        }
    }
    g_free(states);
    return answer;
}
