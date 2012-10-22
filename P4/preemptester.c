/*
 * Preemption tester - works only for RR on each level. 
 */

#include "minithread.h"
#include "synch.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>

long timeout, strt;
int N, temp;
void time_wait(long timeout);
long mytime();
int testthread(int* arg);
int thread1(int* arg);


int main (void){
  minithread_system_initialize(testthread, NULL);
  return 0;
}

int testthread(int* arg){
  int i;
  int id[100];

  temp = 0;
  timeout = 2000; /* in millisceonds */

  for(N = 2; N<=50; N*=5){
    printf("\n\n* %ld threads Test...", N);
    fflush(stdout);
    timeout *= 3;
    strt = mytime();
    id[0]=0;

    for(i = 1; i < N; i++){
      id[i]=i;
      minithread_fork(thread1, &id[i]);
    }

    thread1(&id[0]);

    if(temp >= N*3)
      printf(" Preemption working");
    else 
      printf(" Preemption failed");
    fflush(stdout);
    time_wait(4000); /* in milliseconds */
  }

  return 0;
}

int thread1(int* arg){
  while(temp <= N*3 && strt + timeout > mytime()){
    while(temp % N != *arg)
      ;
    temp++;
  }
  return 0;
}


/* return relative time in millisecs */
long mytime(){
  return clock()/(CLOCKS_PER_SEC/1000);
}

/* time_wait for timeout millisecs */
void time_wait(long to){
  long waitfor = mytime() + to;
  while(waitfor > mytime())
    ;
}
