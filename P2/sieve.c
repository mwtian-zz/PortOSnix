/*
 * Sieve of Eratosthenes application for finding prime numbers
 *
 * This program will print out all the prime numbers less than
 * or equal to MAXPRIME. It works via three different kinds of
 * threads - a producer thread creates numbers and inserts them
 * into a pipeline. A consumer consumes numbers that make it through
 * the pipeline and prints them out as primes. It also creates a new
 * filter thread for each new prime, which subsequently filters out
 * all multiples of that prime from the pipe.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include "minithread.h"
#include "synch.h"

#define MAXPRIME 100

typedef struct {
  int value;
  semaphore_t produce;
  semaphore_t consume;
} channel_t;

typedef struct {
  channel_t* left;
  channel_t* right;
  int prime;
} filter_t;


int max = MAXPRIME;

/* produce all integers from 2 to max */
int source(int* arg) {
  channel_t* c = (channel_t *) arg;
  int i;

  for (i=2; i<=max; i++) {
    c->value = i;
    semaphore_V(c->consume);
    semaphore_P(c->produce);
  }
  
  c->value = -1;
  semaphore_V(c->consume);

  return 0;
}

int filter(int* arg) {
  filter_t* f = (filter_t *) arg;
  int value;

  for (;;) {
    semaphore_P(f->left->consume);
    value = f->left->value;
    semaphore_V(f->left->produce);
    if ((value == -1) || (value % f->prime != 0)) {
      f->right->value = value;
      semaphore_V(f->right->consume);
      semaphore_P(f->right->produce);
    }
    if (value == -1)
      break;
  }

  return 0;
}

int sink(int* arg) {
  channel_t* p = (channel_t *) malloc(sizeof(channel_t));
  int value;

  p->produce = semaphore_create();
  semaphore_initialize(p->produce, 0);
  p->consume = semaphore_create();
  semaphore_initialize(p->consume, 0);

  minithread_fork(source, (int *) p);
  
  for (;;) {
    filter_t* f;

    semaphore_P(p->consume);
    value = p->value;
    semaphore_V(p->produce);
    
    if (value == -1)
      break;

    printf("%d is prime.\n", value);
    
    f = (filter_t *) malloc(sizeof(filter_t));
    f->left = p;
    f->prime = value;
    
    p = (channel_t *) malloc(sizeof(channel_t));
    p->produce = semaphore_create();
    semaphore_initialize(p->produce, 0);
    p->consume = semaphore_create();
    semaphore_initialize(p->consume, 0);
    
    f->right = p;

    minithread_fork(filter, (int *) f);
  }

  return 0;
}

void
main(void) {
  minithread_system_initialize(sink, NULL);
}
