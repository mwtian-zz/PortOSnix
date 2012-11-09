/*
 * Adi
 * Functions that use the process ID to generate random numbers*/

#include<stdio.h>
#include<process.h>
#include "pid_random.h"
#include "random.h"

void mt_random_init(void) {
  sgenrand((unsigned long)_getpid());
}

int mt_random_number(unsigned int max) {
  return  (int)(genintrand(max)-1);
}
