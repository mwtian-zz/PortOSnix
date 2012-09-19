/* Retail shop application and multiple function definitions */
#include <stdlib.h>
#include <stdio.h>
#include "queue.h"
#include "minithread.h"
#include "synch.h"
#include "retail_shop.h"

#define EMPLOYEE_NUM 1000
#define CUSTOMER_NUM 10000

/* Current serial number and IDs */
int serial_num = -1;
int customer_id = 0;
int employee_id = 0;

/* Customer semaphore */
semaphore_t phone_sem;
/* Employee semaphore */
semaphore_t employee_sem;
/* Binary semaphore */
semaphore_t binary_sem;

static int employee(int* arg) {
	int id;
	semaphore_P(employee_sem);
	id = employee_id++;
    printf("Employee %d arrives at the store.\n", id);
    semaphore_V(employee_sem);

	while (1) {
            semaphore_P(employee_sem);
			printf("Employee %d unpacked phone %d\n", id, ++serial_num);
			/* Tell the customer the phone is ready */
			semaphore_V(phone_sem);
			minithread_yield();
	}

	return 0;
}

static int customer(int* arg) {
	int id;
	semaphore_P(binary_sem);
	id = customer_id++;
	printf("Customer %d arrives at the store.\n", id);
    semaphore_V(binary_sem);

    semaphore_P(binary_sem);
	/* Tell employee there is a customer */
	semaphore_V(employee_sem);
	semaphore_P(phone_sem);
	printf("Customer %d got phone %d\n", id, serial_num);
    semaphore_V(binary_sem);
	return 0;
}

static int start(int* arg) {
	int i;
	for (i = 0; i < CUSTOMER_NUM; i++)
        minithread_fork(customer, NULL);
	for (i = 0; i < EMPLOYEE_NUM; i++)
		minithread_fork(employee, NULL);

	return 0;
}

void main(int argc, char** argv) {
	/* Semaphore creation */
	phone_sem = semaphore_create();
	employee_sem = semaphore_create();
	binary_sem = semaphore_create();
	semaphore_initialize(phone_sem, 0);
	semaphore_initialize(employee_sem, 0);
    semaphore_initialize(binary_sem, 1);
	/* Start main thread */
	minithread_system_initialize(start, NULL);
}

