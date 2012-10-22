/*
 * Bounded buffer example.
 *
 * Sample program that implements a single producer-single consumer
 * system. To be used to test the correctness of the threading and
 * synchronization implementations.
 *
 * Change MAXCOUNT to vary the number of items produced by the producer.
 */
#include <stdio.h>
#include <stdlib.h>
#include "minithread.h"
#include "synch.h"
#define BUFFER_SIZE 16

#define MAXCOUNT  100000

int buffer[BUFFER_SIZE];
int size, head, tail;

semaphore_t empty;
semaphore_t full;

unsigned int genintrand(unsigned int maxval);

int consumer(int* arg) {
  int n, i;
  int out = 0;

  while (out < *arg) {
    n = genintrand(BUFFER_SIZE);
    n = (n <= *arg - out) ? n : *arg - out;
    printf("Consumer wants to get %d items out of buffer ...\n", n);
    for (i=0; i<n; i++) {
      semaphore_P(empty);
      out = buffer[tail];
      printf("Consumer is taking %d out of buffer.\n", out);
      tail = (tail + 1) % BUFFER_SIZE;
      size--;
      semaphore_V(full);
    }
  }


  return 0;
}

int producer(int* arg) {
  int count = 1;
  int n, i;

  minithread_fork(consumer, arg);

  minithread_yield();

  while (count <= *arg) {
    n = genintrand(BUFFER_SIZE);
    n = (n <= *arg - count + 1) ? n : *arg - count + 1;
    printf("Producer wants to put %d items into buffer ...\n", n);
    for (i=0; i<n; i++) {
      semaphore_P(full);
      printf("Producer is putting %d into buffer.\n", count);
      buffer[head] = count++;
      head = (head + 1) % BUFFER_SIZE;
      size++;
      semaphore_V(empty);
    }
  }

  return 0;
}

int
main(void) {
  int maxcount = MAXCOUNT;

  size = head = tail = 0;
  empty = semaphore_create();
  semaphore_initialize(empty, 0);
  full = semaphore_create();
  semaphore_initialize(full, BUFFER_SIZE);

  minithread_system_initialize(producer, &maxcount);

  return 0;
}
