#include "minithread.h"
#include <stdio.h>

int
thread0(int *arg) {
    printf("I am thread0...\n");
    printf("I am going to sleep for 1 second...\n");
    printf("hu hu hu...\n");
    minithread_sleep_with_timeout(1000);
    printf("thread0 woke up...\n");

    return 0;
}

int thread1(int *arg) {
    printf("I am thread1...\n");
    printf("I am going to sleep for 2 seconds...\n");
    printf("lu lu lu...\n");
    minithread_sleep_with_timeout(2000);
    printf("thread1 woke up...\n");

    return 0;
}

int thread2(int *arg) {
    printf("I am thread2...\n");
    printf("I am going to sleep for 0 second...\n");
    printf("hu lu lu...\n");
    minithread_sleep_with_timeout(0);
    printf("thread2 woke up...\n");

    return 0;
}

int run(int *arg) {
    minithread_fork(thread0, NULL);
    minithread_fork(thread1, NULL);
    minithread_fork(thread2, NULL);
    return 0;
}

int main() {
    minithread_system_initialize(run, NULL);
    return 0;
}
