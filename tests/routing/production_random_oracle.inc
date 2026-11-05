/*
 * Deterministic, compiled differential test on actual Navit graph objects.
 * The reference solves a two-class directed graph independently using
 * reverse Dijkstra. The synthetic edges have unique road IDs, strictly
 * positive lengths and a 36 km/h profile. Reversal, traffic distortion and
 * destination partial-edge semantics need separate fixtures.
 *
 * Neither the oracle's costs nor its priority order use route_value_seg(),
 * route_turn_* or route_arrival_*.
 */
struct routing_differential_arc {
    int from;
    int cost;
    int restricted;
    int next;
    struct route_graph_segment *segment;
    int original_flags;
};

static unsigned int routing_differential_rand(unsigned int *seed) {
    *seed = *seed * 1664525U + 1013904223U;
    return *seed;
}

static void routing_differential_add_arc(struct routing_differential_arc *arcs,
                                         int *used, int *heads, int from, int to,
                                         int cost, int restricted,
                                         struct route_graph_segment *segment) {
    int i = (*used)++;
    arcs[i].from = from;
    arcs[i].cost = cost;
    arcs[i].restricted = restricted;
    arcs[i].next = heads[to];
    arcs[i].segment = segment;
    arcs[i].original_flags = segment->data.flags;
    heads[to] = i;
}

/* Deliberately independent reverse Dijkstra on 2*n arrival-class states. */
static void routing_differential_oracle(int n, int goal, int penalty,
                                        const int *heads,
                                        const struct routing_differential_arc *arcs,
                                        int *distances) {
    unsigned char *settled = g_new0(unsigned char, 2 * n);
    int i, iteration, current, best, id, arc, incoming_class, edge_class, value;

    for (i = 0; i < 2 * n; ++i)
        distances[i] = INT_MAX;
    distances[2 * goal] = 0;
    distances[2 * goal + 1] = 0;

    for (iteration = 0; iteration < 2 * n; ++iteration) {
        current = -1;
        best = INT_MAX;
        for (i = 0; i < 2 * n; ++i) {
            if (!settled[i] && distances[i] < best) {
                current = i;
                best = distances[i];
            }
        }
        if (current < 0)
            break;
        settled[current] = 1;
        id = current / 2;
        edge_class = current % 2;
        for (arc = heads[id]; arc >= 0; arc = arcs[arc].next) {
            if (arcs[arc].restricted != edge_class)
                continue;
            for (incoming_class = 0; incoming_class <= 1; ++incoming_class) {
                int next = 2 * arcs[arc].from + incoming_class;
                int extra = incoming_class && !edge_class ? penalty : 0;
                if (settled[next] || best > INT_MAX - arcs[arc].cost - extra)
                    continue;
                value = best + arcs[arc].cost + extra;
                if (value < distances[next])
                    distances[next] = value;
            }
        }
    }
    g_free(settled);
}

static int navit_routing_test_differential(void) {
    static const int penalties[] = { 0, 100, 9000 };
    int seed_index, penalty_index, mismatches = 0, trials = 0, update_trials = 0;

    for (seed_index = 0; seed_index < 2000; ++seed_index) {
        for (penalty_index = 0; penalty_index < 3; ++penalty_index) {
            struct route_graph *graph = g_new0(struct route_graph, 1);
            struct vehicleprofile profile = { 0 };
            struct roadprofile road_profile = { 0 };
            struct route_graph_point **points;
            struct route_graph_segment *added, *changed, *last_closed = NULL;
            struct routing_differential_arc *arcs;
            struct route_graph_segment_data data = { 0 };
            struct item item = { 0 };
            unsigned int random_state = (unsigned int)seed_index + 1U;
            int n = 4 + seed_index % 197, max_edges = 3 * n;
            int extra_edges = 0;
            int *heads, *distances, *incremental = NULL, arc_count = 0, edge_count = 0;
            int restriction_rate = seed_index % 3 == 0 ? 10 : (seed_index % 3 == 1 ? 30 : 60);
            int oneway_rate = seed_index % 2 == 0 ? 0 : 20;
            int i, e, from, to, cost, restricted, orientation, expected, observed;

            graph->heap = fh_makekeyheap();
            profile.roadprofile_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
            profile.through_traffic_penalty = penalties[penalty_index];
            profile.flags_forward_mask = AF_ONEWAYREV;
            profile.flags_reverse_mask = AF_ONEWAY;
            road_profile.speed = 36;
            road_profile.maxspeed = 36;
            g_hash_table_insert(profile.roadprofile_hash, (gpointer)(long)type_street_0, &road_profile);

            points = g_new0(struct route_graph_point *, n);
            arcs = g_new0(struct routing_differential_arc, 2 * max_edges);
            heads = g_new(int, n);
            distances = g_new(int, 2 * n);
            for (i = 0; i < n; ++i) {
                struct coord c = { 4000 + i, 7000 + seed_index };
                heads[i] = -1;
                points[i] = route_graph_add_point(graph, &c);
            }

            item.type = type_street_0;
            data.item = &item;
            data.maxspeed = 36;
            /* Spanning tree first, followed by up to 2*n additional roads. */
            extra_edges = (int)(routing_differential_rand(&random_state) % (unsigned int)(2 * n + 1));
            for (e = 0; e < n - 1 + extra_edges; ++e) {
                if (e < n - 1) {
                    to = e + 1;
                    from = (int)(routing_differential_rand(&random_state) % (unsigned int)to);
                } else {
                    from = (int)(routing_differential_rand(&random_state) % (unsigned int)n);
                    to = (int)(routing_differential_rand(&random_state) % (unsigned int)n);
                    if (from == to)
                        continue;
                }
                cost = 1 + (int)(routing_differential_rand(&random_state) % 9U);
                restricted = (int)(routing_differential_rand(&random_state) % 100U) < restriction_rate;
                orientation = (int)(routing_differential_rand(&random_state) % 100U);
                data.len = cost;
                data.flags = AF_SPEED_LIMIT | (restricted ? AF_THROUGH_TRAFFIC_LIMIT : 0);
                if (orientation < oneway_rate)
                    data.flags |= AF_ONEWAY;
                item.id_lo = ++edge_count;
                route_graph_add_segment(graph, points[from], points[to], &data);
                added = points[from]->start;
                if (!(data.flags & AF_ONEWAYREV))
                    routing_differential_add_arc(arcs, &arc_count, heads, from, to, cost, restricted, added);
                if (!(data.flags & AF_ONEWAY))
                    routing_differential_add_arc(arcs, &arc_count, heads, to, from, cost, restricted, added);
            }

            route_graph_seed_destination(graph, &profile, points[n - 1], NULL, 0, 0);
            route_graph_compute_shortest_path(graph, &profile, NULL);
            routing_differential_oracle(n, n - 1, profile.through_traffic_penalty,
                                         heads, arcs, distances);

            for (i = 0; i < n; ++i) {
                int arrival;
                for (arrival = 0; arrival < 2; ++arrival) {
                    expected = distances[2 * i + arrival];
                    observed = arrival && profile.through_traffic_penalty
                               ? points[i]->restricted_value : points[i]->value;
                    if (observed != expected) {
                        fprintf(stderr,
                                "DIFF seed=%d P=%d n=%d node=%d arrival=%d expected=%d actual=%d\n",
                                seed_index, profile.through_traffic_penalty, n, i,
                                arrival, expected, observed);
                        mismatches++;
                        if (mismatches >= 10)
                            break;
                    }
                }
                if (mismatches >= 10)
                    break;
            }
            /* Sequential 1–5-change batches exercise increases, decreases,
             * closures, reopening and zero-cost edges. Check both the
             * independent oracle and a fresh flood after every batch.
             */
            if (seed_index < 128 && !mismatches && graph->route_segments) {
                int round;
                incremental = g_new(int, 2 * n);
                for (round = 1; round <= 5 && !mismatches; ++round) {
                    int change;
                    for (change = 0; change < round; ++change) {
                        int step = (round + change) % 5;
                        int which = (int)(routing_differential_rand(&random_state) % (unsigned int)edge_count);
                        changed = graph->route_segments;
                        while (which-- > 0 && changed->next)
                            changed = changed->next;
                        if (step == 3 && last_closed)
                            changed = last_closed;
                        switch (step) {
                        case 0:
                            if (changed->data.len < INT_MAX - 9)
                                changed->data.len += 5;
                            break;
                        case 1:
                            changed->data.len = changed->data.len > 1 ? changed->data.len - 1 : 1;
                            break;
                        case 2:
                            changed->data.flags |= AF_ONEWAY | AF_ONEWAYREV;
                            last_closed = changed;
                            break;
                        case 3:
                            for (i = 0; i < arc_count; ++i)
                                if (arcs[i].segment == changed) {
                                    changed->data.flags = arcs[i].original_flags;
                                    break;
                                }
                            last_closed = NULL;
                            break;
                        default:
                            changed->data.len = 0;
                            break;
                        }
                        for (i = 0; i < arc_count; ++i) {
                            if (arcs[i].segment == changed) {
                                int closed = (changed->data.flags & (AF_ONEWAY | AF_ONEWAYREV))
                                             == (AF_ONEWAY | AF_ONEWAYREV);
                                /* Navit normalizes a zero-duration segment to one tick. */
                                arcs[i].cost = closed ? INT_MAX : (changed->data.len ? changed->data.len : 1);
                            }
                        }
                        route_graph_point_update(&profile, changed->start, graph->heap);
                        route_graph_point_update(&profile, changed->end, graph->heap);
                    }
                    route_graph_compute_shortest_path(graph, &profile, NULL);
                    routing_differential_oracle(n, n - 1, profile.through_traffic_penalty,
                                                 heads, arcs, distances);
                    for (i = 0; i < n; ++i) {
                        int arrival;
                        for (arrival = 0; arrival <= 1; ++arrival) {
                            expected = distances[2 * i + arrival];
                            observed = arrival && profile.through_traffic_penalty
                                       ? points[i]->restricted_value : points[i]->value;
                            incremental[2 * i + arrival] = observed;
                            if (observed != expected) {
                                fprintf(stderr,
                                        "DIFF incremental seed=%d P=%d round=%d node=%d arrival=%d oracle=%d got=%d\n",
                                        seed_index, profile.through_traffic_penalty, round, i, arrival,
                                        expected, observed);
                                mismatches++;
                            }
                        }
                    }
                    route_graph_reset(graph);
                    route_graph_seed_destination(graph, &profile, points[n - 1], NULL, 0, 0);
                    route_graph_compute_shortest_path(graph, &profile, NULL);
                    for (i = 0; i < n; ++i) {
                        int arrival;
                        for (arrival = 0; arrival <= 1; ++arrival) {
                            expected = distances[2 * i + arrival];
                            observed = arrival && profile.through_traffic_penalty
                                       ? points[i]->restricted_value : points[i]->value;
                            if (observed != expected || observed != incremental[2 * i + arrival]) {
                                fprintf(stderr,
                                        "DIFF fresh seed=%d P=%d round=%d node=%d arrival=%d oracle=%d incremental=%d fresh=%d\n",
                                        seed_index, profile.through_traffic_penalty, round, i, arrival,
                                        expected, incremental[2 * i + arrival], observed);
                                mismatches++;
                            }
                        }
                    }
                    update_trials++;
                }
            }
            trials++;
            route_graph_free_segments(graph);
            route_graph_free_points(graph);
            fh_deleteheap(graph->heap);
            g_hash_table_destroy(profile.roadprofile_hash);
            g_free(graph);
            g_free(points);
            g_free(arcs);
            g_free(heads);
            g_free(distances);
            g_free(incremental);
            if (mismatches >= 10)
                break;
        }
        if (mismatches >= 10)
            break;
    }
    fprintf(stderr, "Routing differential: %d graphs, %d update trials, %d mismatches\n",
            trials, update_trials, mismatches);
    return mismatches ? 1 : 0;
}
