/* Retail shop application and multiple function definitions */
#include <stdlib.h>
#include <stdio.h>
#include "queue.h"
#include "minithread.h"
#include "synch.h"

/* Edit the number of employees (N) and customers (M) here. */
#define EMPLOYEE_NUM 787
#define CUSTOMER_NUM 31564

/* Current serial number and IDs, output starts from 1. */
static int serial_num = 0;
static int customer_id = 0;
static int employee_id = 0;

/* Serial numbers of the currently unpacked phones. */
static int unpacked[EMPLOYEE_NUM + 1];
static int head, tail;

/* Phone semaphore (full semaphore in producer-consumer problem) */
static semaphore_t phone_sem;
/* Employee semaphore (empty semaphore in producer-consumer problem) */
static semaphore_t employee_sem;
/* Customer semaphore, employees only unpack when there are customers */
static semaphore_t customer_sem;
/* Binary semaphore for employees */
static semaphore_t mutex;
/* Binary semaphore for customers */
static semaphore_t customer_mutex;


static int
employee(int* arg) {
    int id;
    semaphore_P(mutex);
    id = ++employee_id;
    printf("Employee %d starts working at the store.\n", id);
    semaphore_V(mutex);

    while (1) {
        /* Wait if there is no customer */
        semaphore_P(customer_sem);
        /* Wait for available checkout counter */
        semaphore_P(employee_sem);

        semaphore_P(mutex);
        head = (head + 1) % (EMPLOYEE_NUM + 1);
        unpacked[head] = ++serial_num;
        printf("Employee %d unpacked phone %d\n", id, unpacked[head]);
        semaphore_V(mutex);

        /* Tell the customer the phone is ready */
        semaphore_V(phone_sem);

        minithread_yield();
    }

    return 0;
}

static int
customer(int* arg) {
    int id;
    semaphore_P(customer_mutex);
    id = ++customer_id;
    printf("Customer %d arrives at the store.\n", id);
    semaphore_V(customer_mutex);

    /* Tell employee there is a customer */
    semaphore_V(customer_sem);
    /* Wait for unpacked phone */
    semaphore_P(phone_sem);

    semaphore_P(mutex);
    tail = (tail + 1) % (EMPLOYEE_NUM + 1);
    printf("Customer %d activated phone %d\n", id, unpacked[tail]);
    semaphore_V(mutex);

    /* Employee finishes serving the customer */
    semaphore_V(employee_sem);

    return 0;
}

static int
start(int* arg) {
    int i;
    for (i = 0; i < EMPLOYEE_NUM; i++)
        minithread_fork(employee, NULL);
    for (i = 0; i < CUSTOMER_NUM; i++)
        minithread_fork(customer, NULL);
    return 0;
}

int
main(int argc, char** argv) {
    /* Initialize the unpacked phone buffer */
    head = 0;
    tail = 0;
    /* Semaphore creation and initialization. */
    phone_sem = semaphore_create();
    employee_sem = semaphore_create();
    customer_sem = semaphore_create();
    mutex = semaphore_create();
    customer_mutex = semaphore_create();

    semaphore_initialize(phone_sem, 0);
    semaphore_initialize(employee_sem, EMPLOYEE_NUM);
    semaphore_initialize(customer_sem, 0);
    semaphore_initialize(mutex, 1);
    semaphore_initialize(customer_mutex, 1);

    /* Start main thread. */
    minithread_system_initialize(start, NULL);

    return 0;
}

