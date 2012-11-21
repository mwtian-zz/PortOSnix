#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "read.h"
#include "read_private.h"
#include "minithread.h"
#include "synch.h"
#include "interrupts.h"
#include  <signal.h>


#define MAX_LINE_LENGTH 512
#define READ_INTERRUPT_TYPE 3

struct kb_line {
	struct kb_line* next;
	char buf[MAX_LINE_LENGTH];
};

struct kb_line* kb_head;
struct kb_line* kb_tail;

semaphore_t new_data;

void read_handler(void* arg) {	
	struct kb_line* node = (struct kb_line*) arg;
	set_interrupt_level(DISABLED);

	if (kb_head == NULL) {
		kb_head = node;
		kb_tail = node;
	} else {
		kb_tail->next = node;
		kb_tail = node;
	}

	set_interrupt_level(ENABLED);
	semaphore_V(new_data);
}

int read_poll(void* arg) {
	
	struct kb_line* new_node;

	while (1) {
		new_node = (struct kb_line*) malloc(sizeof(struct kb_line));
		new_node->next = NULL;

		fgets(new_node->buf, MAX_LINE_LENGTH, stdin);
	
		send_interrupt(READ_INTERRUPT_TYPE, read_handler, new_node);
	}
}


int miniterm_initialize() {
	pthread_t read_thread;
    sigset_t set;
    sigset_t old_set;
    sigfillset(&set);
    sigprocmask(SIG_BLOCK,&set,&old_set);


	kprintf("Starting read interrupts.\n");
    mini_read_handler = read_handler;

	kb_head = NULL;
	kb_tail = NULL;

	new_data = semaphore_create();
	semaphore_initialize(new_data, 0);

    AbortOnCondition(pthread_create(&read_thread, NULL, (void*)read_poll, NULL)!=0,
      "pthread");
 
    sigdelset(&old_set,SIGRTMAX-2);
    sigdelset(&old_set,SIGRTMAX-1);
    pthread_sigmask(SIG_SETMASK,&old_set,NULL);
 
	return 0;
}

int miniterm_read(char* buffer, int len) {
	struct kb_line* old_ptr;
	interrupt_level_t old_level;
	int string_len;

	if (len == 0 || buffer == NULL) return 0;

	semaphore_P(new_data);
	old_level = set_interrupt_level(DISABLED);

	assert(kb_head != NULL);
	string_len = strlen(kb_head->buf);
	strncpy(buffer, kb_head->buf, len <= string_len ? len : string_len);
	buffer[len <= string_len ? len-1 : string_len] = 0;

	old_ptr = kb_head;
	kb_head = kb_head->next;
	free(old_ptr);
	if (kb_head == NULL)
		kb_tail = NULL;
	
	set_interrupt_level(old_level);

	return strlen(buffer);
}
