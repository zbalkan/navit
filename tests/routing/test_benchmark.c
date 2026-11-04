#include "production_hooks.h"
#include <stdio.h>

int navit_routing_benchmark(int n, int repeats, int shortcuts);

int main(void) {
  const int sizes[] = {50, 200, 600};
  unsigned int i;
  int shortcuts;
  int failures = 0;

  printf("scenario,vertices,edges,repeat,implementation,operation,elapsed_ns,"
         "cost\n");
  for (shortcuts = 0; shortcuts <= 1; shortcuts++)
    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
      failures |= navit_routing_benchmark(sizes[i], 25, shortcuts);
  fflush(stdout);
  return failures ? 1 : 0;
}
