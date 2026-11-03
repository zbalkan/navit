#ifndef NAVIT_ROUTING_PRODUCTION_TEST_H
#define NAVIT_ROUTING_PRODUCTION_TEST_H

/* Test-only symbol exported by navit_core when NAVIT_ROUTING_TEST_HOOKS=ON.
 * It is absent in ordinary Navit builds.
 */
int navit_routing_test_case(int case_id);

#endif
