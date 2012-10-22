#include <stdio.h>
#include "read.h"
#include "read_private.h"
#include "minithread.h"
#include "synch.h"
#include "interrupts.h"

#define MAX_LINE_LENGTH 512
#define READ_INTERRUPT_TYPE 3

struct kb_line {
	struct kb_line* next;
	char buf[MAX_LINE_LENGTH];
};

struct kb_line* kb_head;
struct kb_line* kb_tail;

semaphore_t new_data;

int WINAPI read_poll(void* arg) {
	
	struct kb_line* new_node;

	while (1) {
		new_node = (struct kb_line*) malloc(sizeof(struct kb_line));
		new_node->next = NULL;

		fgets(new_node->buf, MAX_LINE_LENGTH, stdin);
	
#ifdef WINCE
		if(new_node->buf[0] != 0) 
		    send_interrupt(READ_INTERRUPT_TYPE, new_node);
#else
		send_interrupt(READ_INTERRUPT_TYPE, new_node);
#endif
	}
}

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

int miniterm_initialize() {
	HANDLE read_thread = NULL;
	DWORD id;

	kprintf("Starting read interrupts.\n");
	register_interrupt(READ_INTERRUPT_TYPE, read_handler, INTERRUPT_DEFER);

	kb_head = NULL;
	kb_tail = NULL;

	new_data = semaphore_create();
	semaphore_initialize(new_data, 0);

	read_thread = CreateThread(NULL, 0, read_poll, NULL, 0, &id); 
	assert(read_thread != NULL); 
  
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