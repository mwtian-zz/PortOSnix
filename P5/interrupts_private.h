#ifndef __INTERRUPTS_H_
#define __INTERRUPTS_H_
/*
 * Interface for interrupt related functions used 
 * by the virtual machine symulator
 *
 * YOU SHOULD NOT [NEED TO] MODIFY THIS FILE.
 */

#include "interrupts.h"


/*
 * Set up the interrupt layer by starting the epoll loop.
 * This is called when the clock handler is installed.
 */
extern int interrupt_layer_init();

/*
 * Handle the signal on the main thread, check the safety
 * conditions and if satisfied, manipulate the stack
 * and context to cause the student's interupt handler
 * to fire.  We insert a frame underneath which contians
 * the state at the time of the interrupt, and we insert
 * a function to pop all of the state off the stack as
 * the return value to the student's interrupt handler.
 */
extern void
handle_interrupt();

extern interrupt_handler_t
mini_clock_handler;

extern interrupt_handler_t
mini_network_handler;

extern interrupt_handler_t
mini_read_handler;

extern interrupt_handler_t
mini_disk_handler;

void send_interrupt(int interrupt_type, interrupt_handler_t handler, void* arg);

#endif
