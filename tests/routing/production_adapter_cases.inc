/* Geometry-level proof for the exact movement-plan adapter. */
static int navit_routing_test_adapter(int case_id) {
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
