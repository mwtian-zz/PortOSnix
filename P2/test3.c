/* test3.c

   Ping-pong between threads using semaphores.
*/
#include <inttypes.h>
#include "minithread.h"
#include "synch.h"

#include <stdio.h>
#include <stdlib.h>

#define COUNT 10000

semaphore_t sem1;
semaphore_t sem2;
int x = 0;

int thread2(int* arg) {
  while (x < COUNT) {
    printf("Thread 2, x = %d.\n", x++);
    semaphore_V(sem1);
    semaphore_P(sem2);
  }

  printf("return from thread2\n");
  return 0;
}

int thread1(int* arg) {
  minithread_fork(thread2, NULL);
  while (x < COUNT) {
    printf("Thread 1, x = %d.\n", x++);
    semaphore_P(sem1);
    semaphore_V(sem2);
  }

  printf("return from thread1\n");
  return 0;
}

int
main() {
  sem1 = semaphore_create();
  semaphore_initialize(sem1, 0);
  sem2 = semaphore_create();
  semaphore_initialize(sem2, 0);
  minithread_system_initialize(thread1, NULL);
  return 0;
}
