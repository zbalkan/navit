/* Independent exact-incoming-movement oracle for production graph objects.
 * Uses raw synthetic road lengths and flags, not route_value_seg(),
 * route_turn_transition_*(), or the production search's heap.
 * Every road has a distinct map item; shared-item penalties and terminal
 * partial roads require separate fixtures.
 */
struct routing_incoming_edge {
    struct route_graph_segment *segment;
    int from;
    int to;
    int direction;
    int length;
    int restricted;
    int closed;
};

static int routing_incoming_reference(const struct routing_incoming_edge *edges,
                                      int edge_count, int start, int goal, int penalty,
                                      int *best_out) {
    int state_count = edge_count + 1;
    int *dist = g_new(int, state_count);
    unsigned char *done = g_new0(unsigned char, state_count);
    int i, j, chosen, current, best, answer = INT_MAX;
    for (i = 0; i < state_count; ++i)
        dist[i] = INT_MAX;
    dist[0] = 0;
    for (;;) {
        best = INT_MAX;
        chosen = -1;
        for (i = 0; i < state_count; ++i) {
            if (!done[i] && dist[i] < best) {
                best = dist[i];
                chosen = i;
            }
        }
        if (chosen < 0)
            break;
        current = chosen ? edges[chosen - 1].to : start;
        if (current == goal) {
            answer = best;
            break;
        }
        done[chosen] = 1;
        for (j = 0; j < edge_count; ++j) {
            int extra = 0, cost, next = j + 1;
            if (edges[j].from != current || edges[j].closed || done[next])
                continue;
            if (chosen) {
                const struct routing_incoming_edge *previous = &edges[chosen - 1];
                if (previous->segment == edges[j].segment)
                    continue;
                if (previous->restricted && !edges[j].restricted)
                    extra = penalty;
            }
            cost = edges[j].length ? edges[j].length : 1;
            if (cost > INT_MAX - extra || best > INT_MAX - cost - extra)
                continue;
            if (best + cost + extra < dist[next])
                dist[next] = best + cost + extra;
        }
    }
    *best_out = answer;
    g_free(done);
    g_free(dist);
    return answer != INT_MAX;
}

static int navit_routing_test_incoming_oracle(void) {
    static const int penalties[] = {0, 100, 9000};
    int seed, p, step, failures = 0, comparisons = 0;
    for (seed = 0; seed < 96 && !failures; ++seed) {
        struct route_graph *graph = g_new0(struct route_graph, 1);
        struct vehicleprofile profile = {0};
        struct roadprofile road_profile = {0};
        struct route_graph_point *points[12];
        struct route_graph_segment *segments[28];
        struct item items[28] = {{0}};
        struct routing_incoming_edge edges[56];
        struct route_graph_segment_data data = {0};
        struct route_turn_path actual = {0};
        unsigned int rng = (unsigned int)seed + 1U;
        int n = 4 + seed % 9, m = n + 4, count = 0, i;
        int start = seed % n, goal = (start + n / 2) % n;
        graph->heap = fh_makekeyheap();
        profile.roadprofile_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
        profile.flags_forward_mask = AF_ONEWAYREV;
        profile.flags_reverse_mask = AF_ONEWAY;
        road_profile.speed = 36;
        road_profile.maxspeed = 36;
        g_hash_table_insert(profile.roadprofile_hash, (gpointer)(long)type_street_0, &road_profile);
        for (i = 0; i < n; ++i) {
            struct coord coord = {25000 + i, 26000 + seed};
            points[i] = route_graph_add_point(graph, &coord);
        }
        for (i = 0; i < m; ++i) {
            int from, to, restricted;
            if (i < n - 1) {
                from = i;
                to = i + 1;
            } else {
                from = (int)(routing_differential_rand(&rng) % (unsigned int)n);
                to = (from + 1 + (int)(routing_differential_rand(&rng) % (unsigned int)(n - 1))) % n;
            }
            restricted = (int)(routing_differential_rand(&rng) % 100U) < (seed % 3 == 0 ? 10 : seed % 3 == 1 ? 30 : 60);
            items[i].type = type_street_0;
            items[i].id_lo = 8000 + i;
            data.item = &items[i];
            data.len = 1 + (int)(routing_differential_rand(&rng) % 9U);
            data.maxspeed = 36;
            data.flags = AF_SPEED_LIMIT | (restricted ? AF_THROUGH_TRAFFIC_LIMIT : 0);
            if ((routing_differential_rand(&rng) % 5U) == 0)
                data.flags |= AF_ONEWAY;
            route_graph_add_segment(graph, points[from], points[to], &data);
            segments[i] = points[from]->start;
            edges[count++] = (struct routing_incoming_edge){segments[i], from, to, 1, data.len, restricted, 0};
            edges[count++] = (struct routing_incoming_edge){segments[i], to, from, -1, data.len, restricted,
                                                                (data.flags & AF_ONEWAY) != 0};
        }
        for (p = 0; p < 3 && !failures; ++p) {
            profile.through_traffic_penalty = penalties[p];
            /* Fresh searches after each mutation; incremental state-space
             * consistency is a separate gate, not asserted by this test.
             */
            for (step = 0; step < 5 && !failures; ++step) {
                int expected, found, got, position = start, sum = 0;
                struct route_graph_segment *previous = NULL;
                unsigned int k;
                if (step) {
                    int target = (seed + p + step) % m;
                    if (step == 1) {
                        segments[target]->data.len += 5;
                    } else if (step == 2) {
                        segments[target]->data.len = 0;
                    } else if (step == 3) {
                        segments[target]->data.flags |= AF_ONEWAY | AF_ONEWAYREV;
                    } else {
                        segments[target]->data.flags &= ~(AF_ONEWAY | AF_ONEWAYREV);
                    }
                }
                for (i = 0; i < count; ++i) {
                    struct route_graph_segment *s = edges[i].segment;
                    int flags = s->data.flags;
                    edges[i].length = s->data.len;
                    edges[i].closed = (flags & (edges[i].direction > 0 ? AF_ONEWAYREV : AF_ONEWAY)) != 0;
                }
                found = routing_incoming_reference(edges, count, start, goal, penalties[p], &expected);
                got = route_turn_graph_path(graph, &profile, points[start], points[goal], &actual);
                if (got != found || (got && actual.cost != expected)) {
                    fprintf(stderr, "INCOMING seed=%d P=%d step=%d expected=%d got=%d found=%d\n",
                            seed, penalties[p], step, expected, actual.cost, got);
                    failures++;
                }
                if (got && !failures) {
                    if (!route_turn_path_is_contiguous(&actual, points[start]))
                        failures++;
                    for (k = 0; k < actual.len && !failures; ++k) {
                        struct route_graph_segment *s = actual.segments[k];
                        int direction = actual.directions[k], edge = -1, extra = 0;
                        for (i = 0; i < count; ++i) {
                            if (edges[i].segment == s && edges[i].direction == direction) {
                                edge = i;
                                break;
                            }
                        }
                        if (edge < 0 || edges[edge].closed || edges[edge].from != position
                            || (previous && previous == s)) {
                            failures++;
                            break;
                        }
                        if (previous && (previous->data.flags & AF_THROUGH_TRAFFIC_LIMIT)
                            && !(s->data.flags & AF_THROUGH_TRAFFIC_LIMIT))
                            extra = penalties[p];
                        if (sum > INT_MAX - edges[edge].length - extra) {
                            failures++;
                            break;
                        }
                        sum += (edges[edge].length ? edges[edge].length : 1) + extra;
                        position = edges[edge].to;
                        previous = s;
                    }
                    if (!failures && (position != goal || sum != actual.cost))
                        failures++;
                }
                if (failures)
                    fprintf(stderr, "INCOMING path seed=%d P=%d step=%d cost=%d summed=%d\n",
                            seed, penalties[p], step, actual.cost, sum);
                route_turn_path_clear(&actual);
                comparisons++;
            }
        }
        route_graph_free_segments(graph);
        route_graph_free_points(graph);
        fh_deleteheap(graph->heap);
        g_hash_table_destroy(profile.roadprofile_hash);
        g_free(graph);
    }
    fprintf(stderr, "Exact incoming oracle: %d comparisons, %d failures\n", comparisons, failures);
    return failures ? 1 : 0;
}
