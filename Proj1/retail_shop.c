/* Retail shop application and multiple function definitions */

#include <stdio.h>
#include "queue.h"
#include "synch.h"
#include "retail_shop.h"

/* Current serial number, starting from 0*/
int serial_num = 0;

/* Phone queue. Unpacked by employee */
queue_t phone_queue;

/* Employee number */
int employee_num = 5;

/* Customer number */
int customer_num = 10;

/* Serial number semphore */
semaphore_t serial_sem;

/*Phone queue semphore */
semaphore_t queue_sem;

semaphore_t available_sem;

phone_t phone_create() {
	phone_t new_phone = (phone_t) malloc(sizeof(struct phone));
	if (new_phone == NULL) {
		printf("Can't unpack a new phone\n");
		return NULL;
	}
	
	semaphore_P(serial_sem);
	new_phone->serial_num = serial_num++;
	semaphore_V(serial_sem);
	
	return new_phone;
}

static void employee(int* arg) {
	phone_t new_phone = NULL;
	
	while (1) {
		new_phone = phone_create();
		if (new_phone) {
			semaphore_P(queue_sem);
			queue_append(phone_queue, new_phone);
			printf("Employee %d unpacked phone %d\n", *arg, new_phone->serial_num);
			semaphore_V(queue_sem);
			semaphore_V(available_sem);
		}
	}
}

static void customer(int* arg) {
	phone_t new_phone = NULL;
	int i;
	
	printf("Customer is running\n");
	for (i = 0; i < customer_num; i++) {
		while (new_phone == NULL) {
			semaphore_P(available_sem);
			semaphore_P(queue_sem);
			queue_dequeue(phone_queue, (void**)&new_phone);
			semaphore_V(queue_sem);
		}

		printf("Customer %d got phone %d\n", *arg, new_phone->serial_num);
		free(new_phone);
	}
}

static void start(int* arg) {
	int i;
	
	minithread_fork(customer, NULL);
	//minithread_yield();
	
	for (i = 0; i < employee_num; i++) {
		minithread_fork(employee, &i);
	}
	
}

void main(int argc, char** argv) {
	phone_queue = queue_new();
	if (phone_queue == NULL) {
		printf("Can't create phone queue!\n");
		return;
	}
	
	serial_sem = semaphore_create();
	queue_sem = semaphore_create();
	available_sem = semaphore_create();
	semaphore_initialize(serial_sem, 1);
	semaphore_initialize(queue_sem, 1);
	semaphore_initialize(available_sem, 0);
	
	minithread_system_initialize(start, NULL);
}