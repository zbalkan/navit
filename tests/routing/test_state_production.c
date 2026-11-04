#include "production_hooks.h"
#include <stdio.h>

int main(void) {
  const int cases[] = {7, 8, 9};
  unsigned int i;
  int failures = 0;
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    if (navit_routing_test_case(cases[i]) != 0)
      ++failures;
    else
      printf("PASS turn-aware candidate case %d\n", cases[i]);
  }
  return failures ? 1 : 0;
}
