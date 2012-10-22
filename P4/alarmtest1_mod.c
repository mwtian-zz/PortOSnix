/*
 * Spawn three threads and get them to sleep.
*/

#include "minithread.h"

#include <stdio.h>
#include <stdlib.h>

extern long ticks;

int thread3(int* arg) {
  printf("Thread 3 executes and finishes.\n");

  return 0;
}

int thread2(int* arg) {
    minithread_t thread = minithread_fork(thread3, NULL);
    printf("Thread 2 starts.\n");
	minithread_sleep_with_timeout(10000); /* ten seconds */
	printf("fired at %ld ticks.\n", ticks);
    printf("Thread 2 just woke up and finishes\n");

  return 0;
}

int thread1(int* arg) {
    minithread_t thread = minithread_fork(thread2, NULL);
    printf("Thread 1 starts.\n");
    minithread_sleep_with_timeout(5000); /* five seconds */
	printf("fired at %ld ticks.\n", ticks);
    printf("Thread 1 just woke up, and is going to sleep again.\n");
	minithread_sleep_with_timeout(15000); /* fifteen seconds */
	printf("fired at %ld ticks.\n", ticks);
    printf("Thread 1 just woke up and finishes\n");

  return 0;
}

void main(void) {
  minithread_system_initialize(thread1, NULL);
}
