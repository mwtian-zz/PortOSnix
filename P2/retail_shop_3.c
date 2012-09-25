/* Retail shop application and multiple function definitions */
#include <stdlib.h>
#include <stdio.h>
#include "queue.h"
#include "minithread.h"
#include "synch.h"

/* Edit the number of employees (N) and customers (M) here. */
#define EMPLOYEE_NUM 789
#define CUSTOMER_NUM 3178

/* Current serial number and IDs, starts from 1. */
static int serial_num = 0;
static int customer_id = 0;
static int employee_id = 0;

/* Buffer to store serial numbers of the currently unpacked phones. */
static int unpacked[EMPLOYEE_NUM + 1];
static int head, tail;

/* full semaphore in producer-consumer problem */
static semaphore_t full_sem;
/* empty semaphore in producer-consumer problem */
static semaphore_t empty_sem;
/* Customer semaphore, employees only unpack when there are customers */
static semaphore_t customer_sem;
/* Binary semaphore for employees */
static semaphore_t employee_mutex;
/* Binary semaphore for customers */
static semaphore_t customer_mutex;


static int
employee(int* arg)
{
    int id;
    semaphore_P(employee_mutex);
    id = ++employee_id;
    printf("Employee %d starts working at the store.\n", id);
    semaphore_V(employee_mutex);

    while (1) {
        /* Wait if there is no customer */
        semaphore_P(customer_sem);
        /* Wait for available checkout counter */
        semaphore_P(empty_sem);

        semaphore_P(employee_mutex);
        head = (head + 1) % (EMPLOYEE_NUM + 1);
        unpacked[head] = ++serial_num;
        printf("Employee %d unpacked phone %d\n", id, unpacked[head]);
        semaphore_V(employee_mutex);

        /* Tell the customer the phone is ready */
        semaphore_V(full_sem);
    }

    return 0;
}

static int
customer(int* arg)
{
    int id;
    semaphore_P(customer_mutex);
    id = ++customer_id;
    printf("Customer %d arrives at the store.\n", id);
    semaphore_V(customer_mutex);

    /* Tell employee there is a customer */
    semaphore_V(customer_sem);
    /* Wait for unpacked phone */
    semaphore_P(full_sem);

    semaphore_P(customer_mutex);
    tail = (tail + 1) % (EMPLOYEE_NUM + 1);
    printf("Customer %d activated phone %d\n", id, unpacked[tail]);
    semaphore_V(customer_mutex);

    /* Employee finishes serving the customer */
    semaphore_V(empty_sem);

    return 0;
}

static int
start(int* arg)
{
    int i;
    for (i = 0; i < EMPLOYEE_NUM; i++)
        minithread_fork(employee, NULL);
    for (i = 0; i < CUSTOMER_NUM; i++)
        minithread_fork(customer, NULL);
    return 0;
}

int
main(int argc, char** argv)
{
    /* Initialize the unpacked phone buffer */
    head = 0;
    tail = 0;
    /* Semaphore creation and initialization. */
    full_sem = semaphore_create();
    empty_sem = semaphore_create();
    customer_sem = semaphore_create();
    employee_mutex = semaphore_create();
    customer_mutex = semaphore_create();

    semaphore_initialize(full_sem, 0);
    semaphore_initialize(empty_sem, EMPLOYEE_NUM);
    semaphore_initialize(customer_sem, 0);
    semaphore_initialize(employee_mutex, 1);
    semaphore_initialize(customer_mutex, 1);

    /* Start main thread. */
    minithread_system_initialize(start, NULL);

    return 0;
}

