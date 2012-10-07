/* Retail shop application and multiple function definitions */
#include <stdlib.h>
#include <stdio.h>
#include "queue.h"
#include "minithread.h"
#include "synch.h"
#include "retail_shop_1.h"

/* Current serial number, starting from 0*/
int serial_num = 0;

/* Phone queue. Unpacked by employee */
queue_t phone_queue;

/* Employee number */
int employee_num = 8887;
/* Customer number */
int customer_num = 20980;
/* Employee id */
int employee_id = 0;
/*Customer id */
int customer_id = 0;
/* Empty semaphore */
semaphore_t empty_sem;
/* Full semaphore */
semaphore_t full_sem;
/* Customer semaphore */
semaphore_t customer_sem;

phone_t
phone_create() {
    phone_t new_phone = (phone_t) malloc(sizeof(struct phone));
    if (new_phone == NULL) {
        printf("Can't unpack a new phone\n");
        return NULL;
    }

    new_phone->serial_num = serial_num++;
    return new_phone;
}

static int
employee(int* arg) {
    phone_t new_phone = NULL;
    int id = employee_id++;

    while (1) {
        /* Check if there is any customer in need of a phone */
        semaphore_P(customer_sem);

        /* Wait if buffer full */
        semaphore_P(empty_sem);
        new_phone = phone_create();
        if (NULL != new_phone) {
            queue_append(phone_queue, new_phone);
            printf("Employee %d unpacked phone %d\n", id, new_phone->serial_num);
        }
        /* Signal a phone is ready */
        semaphore_V(full_sem);

        /* minithread_yield(); */
    }

    return 0;
}

static int
customer(int* arg) {
    phone_t new_phone = NULL;
    int id = customer_id++;

    printf("Customer %d is waiting for a phone\n", id);

    /* Signal employee there is a customer */
    semaphore_V(customer_sem);

    /* Wait if no unpacked phone */
    semaphore_P(full_sem);
    queue_dequeue(phone_queue, (void**)&new_phone);
    printf("Customer %d got phone %d\n", id, new_phone->serial_num);
    free(new_phone);
    /* Signal a buffer slot is open */
    semaphore_V(empty_sem);

    return 0;
}

static int
start(int* arg) {
    int i;

    for (i = 0; i < employee_num; i++) {
        minithread_fork(employee, NULL);
    }

    for (i = 0; i < customer_num; i++) {
        minithread_fork(customer, NULL);
    }

    return 0;
}

int
main(int argc, char** argv) {
    phone_queue = queue_new();
    if (phone_queue == NULL) {
        printf("Can't create phone queue!\n");
        return 0;
    }

    /* Semaphore creation */
    empty_sem = semaphore_create();
    full_sem = semaphore_create();
    customer_sem = semaphore_create();
    semaphore_initialize(empty_sem, employee_num);
    semaphore_initialize(full_sem, 0);
    semaphore_initialize(customer_sem, 0);

    /* Start main thread */
    minithread_system_initialize(start, NULL);

    return 0;
}
