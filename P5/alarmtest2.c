/* 
 *  Spawn threads and get them to sleep for random amounts of time.
*/

#include "minithread.h"

#include <stdio.h>
#include <stdlib.h>

/* sleep for at most five seconds (five thousand milliseconds) */
#define MAX_SLEEP_TIME 1000
#define N_SLEEPS 1000
#define N_THREADS 10
#define SEED 32

int thread_numbers[N_THREADS];

int sleeper(int* number) {
  int i, delay;

  printf("Thread %d starts.\n", *number);
  for (i=0; i<N_SLEEPS; i++) {
    delay = rand() % MAX_SLEEP_TIME + 1;
    printf("Thread %d going to sleep for %d milliseconds ...\n", 
	   *number, delay);
    minithread_sleep_with_timeout(delay);
    printf("Thread %d woke up.\n", *number);
  }

  printf("Thread %d exiting.\n", *number);

  return 0;
}

int startup(int* arg) {
  minithread_t thread;
  int i;

  srand(SEED);

  for (i=0; i<N_THREADS; i++)
    thread_numbers[i] = i+1;

  for (i=0; i<N_THREADS-1; i++)
    thread = minithread_fork(sleeper, &(thread_numbers[i]));

  sleeper(&(thread_numbers[N_THREADS-1]));

  return 0;
}

void main(void) {
  minithread_system_initialize(startup, NULL);
}
