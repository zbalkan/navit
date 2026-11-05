/*
 * Two-label incremental propagation for through-traffic transitions.
 * A graph junction has a distinct continuation cost when the vehicle
 * arrives from a through-traffic-restricted segment. Both labels reuse
 * Navit's destination-oriented incremental queues.
 *
 * This does not model arbitrary turn histories; explicit turn restrictions
 * remain represented by Navit's existing expanded restriction graph.
 */
static int route_arrival_terminal(struct vehicleprofile *profile,
                                  struct route_graph_point *p, int restricted) {
    (void)profile;
    return restricted ? p->restricted_dst_val : p->dst_val;
}

static int route_arrival_step(struct vehicleprofile *profile,
                              struct route_graph_segment *s, int direction,
                              struct route_graph_point *neighbor,
                              int incoming_restricted) {
    int val, successor_cost, extra = 0;
    struct route_graph_segment *successor;
    int outgoing_restricted = !route_through_traffic_allowed(profile, s);

    /* A self-loop is not a legal successor in the production graph. */
    if (s->start == s->end)
        return INT_MAX;
    val = route_value_seg_base(profile, s, direction);
    if (val == INT_MAX)
        return INT_MAX;
    successor_cost = outgoing_restricted ? neighbor->restricted_value : neighbor->value;
    successor = outgoing_restricted ? neighbor->restricted_seg : neighbor->seg;
    /* Reversing the exact same segment is forbidden even with penalty2. */
    if (successor == s)
        return INT_MAX;
    if (successor && item_is_equal(s->data.item, successor->data.item)) {
        if (profile->turn_around_penalty2 <= 0)
            return INT_MAX;
        extra = profile->turn_around_penalty2;
    }
    if (incoming_restricted && !outgoing_restricted) {
        if (profile->through_traffic_penalty < 0
            || extra > INT_MAX - profile->through_traffic_penalty)
            return INT_MAX;
        extra += profile->through_traffic_penalty;
    }
    if (val > INT_MAX - extra || successor_cost == INT_MAX
        || val + extra > INT_MAX - successor_cost)
        return INT_MAX;
    return val + extra + successor_cost;
}

static void route_arrival_update(struct vehicleprofile *profile,
                                 struct route_graph_point *p,
                                 struct fibheap *heap) {
    struct route_graph_segment *s;
    struct route_graph_segment *old_normal = p->seg;
    struct route_graph_segment *old_restricted = p->restricted_seg;
    int normal, restricted;

    p->rhs = route_arrival_terminal(profile, p, 0);
    p->restricted_rhs = route_arrival_terminal(profile, p, 1);
    p->seg = p->dst_seg;
    p->restricted_seg = p->restricted_dst_seg;

    for (s = p->start; s; s = s->start_next) {
        if (s->start == s->end)
            continue;
        normal = route_arrival_step(profile, s, 1, s->end, 0);
        restricted = route_arrival_step(profile, s, 1, s->end, 1);
        if (normal < p->rhs) {
            p->rhs = normal;
            p->seg = s;
        }
        if (restricted < p->restricted_rhs) {
            p->restricted_rhs = restricted;
            p->restricted_seg = s;
        }
    }
    for (s = p->end; s; s = s->end_next) {
        if (s->start == s->end)
            continue;
        normal = route_arrival_step(profile, s, -1, s->start, 0);
        restricted = route_arrival_step(profile, s, -1, s->start, 1);
        if (normal < p->rhs) {
            p->rhs = normal;
            p->seg = s;
        }
        if (restricted < p->restricted_rhs) {
            p->restricted_rhs = restricted;
            p->restricted_seg = s;
        }
    }

    if (p->el) {
        fh_delete(heap, p->el);
        p->el = NULL;
    }
    if (old_normal != p->seg)
        p->policy_dirty = 1;
    if (p->rhs != p->value || p->policy_dirty)
        p->el = fh_insertkey(heap, MIN(p->rhs, p->value), &p->queue[0]);

    if (p->restricted_el) {
        fh_delete(heap, p->restricted_el);
        p->restricted_el = NULL;
    }
    if (old_restricted != p->restricted_seg)
        p->restricted_policy_dirty = 1;
    if (p->restricted_rhs != p->restricted_value || p->restricted_policy_dirty)
        p->restricted_el = fh_insertkey(heap, MIN(p->restricted_rhs, p->restricted_value), &p->queue[1]);
}
