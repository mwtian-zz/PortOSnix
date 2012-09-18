#ifndef __INTERRUPTS_H_
#define __INTERRUPTS_H_
/*
 * Interface for interrupt related functions used 
 * by the virtual machine symulator
 *
 * YOU SHOULD NOT [NEED TO] MODIFY THIS FILE.
 */

#include "interrupts.h"

#define CLOCK_INTERRUPT_TYPE 1

/*
 * Controls the behavior when the interrupt handler is interrupted
 * by an other interrupt.
 */
typedef enum {INTERRUPT_DROP,INTERRUPT_DEFER}interrupt_property_t;

/* 
 * Infinite loop.
 */
extern void loopforever(void);

/*
 * send an interrupt to the system thread: adjust its stack to call
 * the appropriate interrupt handler with the specified argument. the
 * "type" argument identifies interrupt-specific processing which must
 * be done, in particular, what we should do if interrupts are
 * disabled or the system thread is in a non-preemptable state
 * (e.g. executing a system library function). clock interrupts are
 * dropped, network interrupts are deferred. this function replaces
 * code which used to be common to clock_poll and network_poll.
 */
void send_interrupt(int type, void* arg);

/* register an interrupt handler. Any interrupt has to be registered before
 * used. Interrupts will start to be received after interrupt
 * registration.
 *
 * Returns 0 if successfull, -1 otherwise (interrupt already registered)
*/
int register_interrupt(int type, interrupt_handler_t handler, 
		       interrupt_property_t property);

#endif  __INTERRUPTS_H_
