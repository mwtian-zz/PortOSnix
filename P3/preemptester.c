/*
 * Preemption tester - works only for RR on each level. The first 3 three cases  * must work
 */

#include "minithread.h"
#include "synch.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

int temp;
int temp2;
long timeout, strt;

int N;

void wait(long timeout);
long mytime();

int testthread(int* arg);
int thread1(int *arg);
//int thread2(int *arg);


void main (void){
	
  minithread_system_initialize(testthread, NULL);
	
}

int testthread(int* arg){

  minithread_t thrdid1;//, thrdid2;
  int id[100];
  int i;

  temp = 0;
  temp2 = 1;
	
  // Two threads

  //for(;;){
    timeout = 2000;
    for(N = 2; N<=50; N*=5){
      printf("\n\n* %d threads Test...", N);
      strt = mytime();
      timeout *= 3;

      id[0] = 0;

      for(i = 1; i <= N-1; i++){
	id[i] = i;
	thrdid1 = minithread_fork(thread1, &(id[i]));
      }
	
      thread1(&(id[0]));

      if(temp >= N*3)
	printf(" Preemption working");
      else 
	printf(" Preemption failed");

      wait(4000);
    }
  //}


  return 0;
}

int thread1(int *arg){

	
  while(temp <= N*3 && strt+ timeout > mytime()){
    while(temp % N != (*arg) )
      ;
    temp++;
    //	printf(".thread %d.", *arg);
  }
	

  return 0;

}


// return relative time in millisecs
long mytime(){

  long i = ((long) clock());

  return i;

}

// wait for timeout millisecs
void wait(long timeout){

  long now = mytime();

  while(now+timeout > mytime())
    ;
}
