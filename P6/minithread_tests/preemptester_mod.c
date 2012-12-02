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

int id[100];

int
main (void)
{
    minithread_system_initialize(testthread, NULL);
    return 0;
}

int
testthread(int* arg)
{
    int i;
    temp = 0;
    temp2 = 1;
    timeout = 1000;

    for (N = 2; N <= 50; N *= 5) {
        printf("%d threads Test...", N);
        strt = mytime();
        timeout *= 3;
        id[0] = 0;

        for(i = 1; i <= N-1; i++) {
            id[i] = i;
            minithread_fork(thread1, &(id[i]));
        }
        thread1(&(id[0]));
        if (temp >= N*3)
            printf(" Preemption working\n");
        else
            printf(" Preemption failed\n");

        wait(4000);
        //minithread_sleep_with_timeout(8000);
    }

    return 0;
}

int
thread1(int *arg)
{
    while (temp <= N * 3 && strt + timeout > mytime()) {
        while (temp % N != (*arg) && temp <= N * 3)
            ;
        temp++;
    }
    return 0;

}

// return relative time in millisecs
long mytime()
{
    long i = ((long) clock() / (CLOCKS_PER_SEC / 1000));
    return i;
}

// wait for timeout millisecs
void wait(long timeout)
{
    long now = mytime();
    while(now+timeout > mytime())
        ;
}
