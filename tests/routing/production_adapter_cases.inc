/* Three-segment route geometry is checked with a reusable middle segment,
 * so the test does not require an external map provider.
 */
static int navit_routing_test_adapter_multisegment(void) {
    struct route_graph *graph = g_new0(struct route_graph, 1);
    struct vehicleprofile profile = { 0 };
    struct roadprofile road_profile = { 0 };
    struct route_graph_segment_data data = { 0 };
    struct item item = { 0 };
    struct coord coords[] = { { 9001, 9001 }, { 9002, 9001 }, { 9003, 9001 }, { 9004, 9001 } };
    struct route_graph_point *a, *b, *c, *d;
    struct route_graph_segment *bd;
    struct street_data *start_street, *dest_street;
    struct route_info pos = { 0 }, dst = { 0 };
    struct route_path *oldpath, *result;
    struct route_path_segment *first, *middle, *last;
    int success;

    graph->heap = fh_makekeyheap();
    profile.roadprofile_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
    profile.through_traffic_penalty = 100;
    road_profile.speed = 36;
    road_profile.maxspeed = 36;
    g_hash_table_insert(profile.roadprofile_hash, (gpointer)(long)type_street_0, &road_profile);
    a = route_graph_add_point(graph, &coords[0]);
    b = route_graph_add_point(graph, &coords[1]);
    c = route_graph_add_point(graph, &coords[2]);
    d = route_graph_add_point(graph, &coords[3]);
    item.type = type_street_0;
    item.id_lo = 101;
    data.item = &item;
    data.flags = AF_SPEED_LIMIT | AF_ROUNDABOUT;
    data.maxspeed = 36;
    data.len = 1;
    route_graph_add_segment(graph, a, b, &data);
    route_graph_add_segment(graph, c, b, &data);
    item.id_lo = 102;
    data.len = 2;
    route_graph_add_segment(graph, b, d, &data);
    bd = b->start;
    item.id_lo = 103;
    route_graph_add_segment(graph, d, c, &data);

    start_street = g_malloc0(sizeof(*start_street) + 2 * sizeof(struct coord));
    dest_street = g_malloc0(sizeof(*dest_street) + 2 * sizeof(struct coord));
    start_street->item = a->start->data.item;
    start_street->count = 2;
    start_street->c[0] = a->c;
    start_street->c[1] = b->c;
    dest_street->item = d->start->data.item;
    dest_street->count = 2;
    dest_street->c[0] = d->c;
    dest_street->c[1] = c->c;
    pos.street = start_street;
    pos.pos = 0;
    pos.percent = 0;
    pos.street_direction = 1;
    pos.lp = a->c;
    pos.c = a->c;
    pos.lenpos = 1;
    dst.street = dest_street;
    dst.pos = 0;
    dst.percent = 100;
    dst.lp = c->c;
    dst.c = c->c;
    dst.lenneg = 2;
    route_graph_init(graph, &dst, &profile);

    /* Reuse just the middle segment from an existing path, exactly as
     * production route_path_add_item_from_graph() can reuse old geometry.
     */
    oldpath = g_new0(struct route_path, 1);
    /* route_path_destroy decrements in_use before freeing the path object. */
    oldpath->in_use = 1;
    oldpath->path_hash = item_hash_new();
    route_path_add_line(oldpath, &b->c, &d->c, 2);
    oldpath->path->direction = 1;
    oldpath->path->data->item = bd->data.item;
    item_hash_insert(oldpath->path_hash, &bd->data.item, oldpath->path);

    result = route_path_new_movement(graph, oldpath, &pos, &dst, &profile);
    first = result ? result->path : NULL;
    middle = first ? first->next : NULL;
    last = middle ? middle->next : NULL;
    success = first && middle && last && !last->next;
    if (success)
        success = first->direction == 1 && middle->direction == 1 && last->direction == 1
                  && first->c[0].x == a->c.x
                  && first->c[first->ncoords - 1].x == b->c.x
                  && middle->c[0].x == b->c.x
                  && middle->c[middle->ncoords - 1].x == d->c.x
                  && last->c[0].x == d->c.x
                  && last->c[last->ncoords - 1].x == c->c.x;
    if (!success)
        fprintf(stderr, "movement geometry adapter case 47: invalid multi-segment route geometry\n");
    if (result)
        route_path_destroy(result, 0);
    route_path_destroy(oldpath, 0);
    g_free(start_street);
    g_free(dest_street);
    route_graph_free_segments(graph);
    route_graph_free_points(graph);
    fh_deleteheap(graph->heap);
    g_hash_table_destroy(profile.roadprofile_hash);
    g_free(graph);
    return success ? 0 : 1;
}

/* Geometry-level proof for the exact movement-plan adapter. */
static int navit_routing_test_adapter(int case_id) {
    if (case_id == 47)
        return navit_routing_test_adapter_multisegment();
    struct route_graph *graph = g_new0(struct route_graph, 1);
    struct vehicleprofile profile = { 0 };
    struct roadprofile road_profile = { 0 };
    struct route_graph_segment_data data = { 0 };
    struct item item = { 0 };
    struct coord acoord = { 100, 100 }, bcoord = { 200, 100 };
    struct route_graph_point *a, *b;
    struct street_data *street = g_malloc0(sizeof(*street) + 2 * sizeof(struct coord));
    struct route_info pos = { 0 }, dst = { 0 };
    struct route_path *path;
    int result;

    graph->heap = fh_makekeyheap();
    profile.roadprofile_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
    profile.through_traffic_penalty = 9000;
    road_profile.speed = 36;
    road_profile.maxspeed = 36;
    g_hash_table_insert(profile.roadprofile_hash, (gpointer)(long)type_street_0, &road_profile);

    item.type = type_street_0;
    item.id_lo = 4201;
    street->item = item;
    street->count = 2;
    street->c[0] = acoord;
    street->c[1] = bcoord;
    data.item = &item;
    data.maxspeed = 36;
    data.flags = AF_SPEED_LIMIT | AF_ROUNDABOUT;
    data.len = 100;

    a = route_graph_add_point(graph, &acoord);
    b = route_graph_add_point(graph, &bcoord);
    route_graph_add_segment(graph, a, b, &data);

    pos.street = street;
    pos.percent = 20;
    pos.pos = 0;
    pos.street_direction = 1;
    pos.lenneg = 20;
    pos.lenpos = 80;
    pos.lp.x = 120;
    pos.lp.y = 100;
    pos.c = pos.lp;

    dst.street = street;
    dst.percent = 80;
    dst.pos = 0;
    dst.lenneg = 80;
    dst.lenpos = 20;
    dst.lp.x = 180;
    dst.lp.y = 100;
    dst.c = dst.lp;

    path = case_id == 42 ? route_path_new_movement(graph, NULL, &pos, &dst, &profile) : NULL;
    result = path && path->path && !path->path->next;
    if (result)
        result = path->path->data->len == 60 && path->path->direction == 1
                 && path->path->ncoords == 2
                 && path->path->c[0].x == 120 && path->path->c[1].x == 180;
    if (!result)
        fprintf(stderr, "movement geometry adapter case %d: direct same-segment geometry failed\n", case_id);

    if (path)
        route_path_destroy(path, 0);
    route_graph_free_segments(graph);
    route_graph_free_points(graph);
    fh_deleteheap(graph->heap);
    g_hash_table_destroy(profile.roadprofile_hash);
    g_free(street);
    g_free(graph);
    return result ? 0 : 1;
}
