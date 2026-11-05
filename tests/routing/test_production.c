#include "production_hooks.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  int legacy_smoke = argc == 2 && strcmp(argv[1], "--legacy-smoke") == 0;
  if (argc > 1 && !legacy_smoke) {
    fprintf(stderr, "Usage: navit_routing_production_test [--legacy-smoke]\n");
    return 2;
  }
  printf("routing-point-size=%d\n", navit_routing_test_point_size());
  /* 29: INCR-006a scratch consistency; 31–33: default P=9000. */
  const int cases[] = {1,  2,  3,  4,  5,  6,  29, 31, 32,
                       33, 34, 35, 36, 37, 38, 39, 40, 41};
  int failures = 0;
  unsigned int i;
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    if (legacy_smoke && cases[i] > 3)
      continue;
    if (navit_routing_test_case(cases[i]) != 0)
      ++failures;
    else
      printf("PASS production routing case %d\n", cases[i]);
  }
  return failures ? 1 : 0;
}
