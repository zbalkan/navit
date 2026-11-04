#include "production_hooks.h"
#include <stdio.h>

int main(void) {
  const int cases[] = {1, 2, 3, 4, 5, 6};
  int failures = 0;
  unsigned int i;
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    if (navit_routing_test_case(cases[i]) != 0)
      ++failures;
    else
      printf("PASS production routing case %d\n", cases[i]);
  }
  return failures ? 1 : 0;
}
