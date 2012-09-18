/* test1.c

   Spawn a single thread.
*/

#include "minithread.h"

#include <stdio.h>
#include <stdlib.h>


int
thread(int* arg) {
  printf("Hello, world!\n");

  return 0;
}

void 
main(void) {
  minithread_system_initialize(thread, NULL);
}
