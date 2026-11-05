#include "production_hooks.h"
#include <stdio.h>

int main(void) {
  const int cases[] = {13, 14, 15, 16, 17, 18, 19, 20,
                       21, 22, 23, 24, 25, 26, 27, 28};
  unsigned int i;
  int failures = 0;
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    fprintf(stderr, "RUN movement-state semantics %d\n", cases[i]);
    if (navit_routing_test_case(cases[i]) != 0)
      failures++;
    else
      printf("PASS movement-state semantics %d\n", cases[i]);
  }
  return failures ? 1 : 0;
}
