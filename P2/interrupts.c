#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include "interrupts_private.h"
#include "minithread.h"
#include "assert.h"
#include "machineprimitives.h"

#define MAXEVENTS 64
#define NETWORK_INTERRUPT 0
#define CLOCK_INTERRUPT 1
#define MAXBUF 1000
#define ENABLED 1
#define DISABLED 0

interrupt_level_t interrupt_level;
long ticks;
extern int start();
extern int end();

#define R8 0
#define R9 1
#define R10 2
#define R11 3
#define R12 4
#define R13 5
#define R14 6
#define R15 7
#define RDI 8
#define RSI 9
#define RBP 10
#define RBX 11
#define RDX 12
#define RAX 13
#define RCX 14
#define RSP 15
#define RIP 16
#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
       } while (0)



/*                       top of interrupted stack
 *   .-----------------./
 *   | return address  | - (A) interrupted rip
 *   |-----------------|
 *   | padding         | - (C) need to align to 16bytes
 *   |-----------------|
 *   | saved registers |
 *   |       .         |
 *   |       .         |
 *   |       .         |
 *   |-----------------|
 *   | return address  | - (D) trampoline function
 *   +-----------------+
 *
 */

interrupt_handler_t mini_clock_handler;

/*
 * atomically sets interrupt level and returns the original
 * interrupt level
 */
interrupt_level_t set_interrupt_level(interrupt_level_t newlevel) {
    return swap(&interrupt_level, newlevel);
}


/*
 * Register the minithread clock handler by making
 * mini_clock_handler point to it.
 *
 * Then set the signal handler for SIGRTMIN+1 to 
 * handle_interrupt.  This signal handler will either
 * interrupt the minithreads, or drop the interrupt,
 * depending on safety conditions.
 *
 * The signals are handled on their own stack to reduce
 * chances of an overrun.
 */
void 
minithread_clock_init(interrupt_handler_t clock_handler){
    timer_t timerid;
    struct sigevent sev;
    struct itimerspec its;
    long long freq_nanosecs;
    sigset_t mask;
    struct sigaction sa;
    stack_t ss;
    mini_clock_handler = clock_handler;

    ss.ss_sp = malloc(SIGSTKSZ);
    if (ss.ss_sp == NULL){
        perror("malloc."); 
        abort();
    }
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;
    if (sigaltstack(&ss, NULL) == -1){
        perror("signal stack");
        abort();
    }


    /* Establish handler for timer signal */
    sa.sa_handler = (void*)handle_interrupt;
    sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK; 
    sa.sa_sigaction= (void*)handle_interrupt;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGRTMIN+1, &sa, NULL) == -1)
        errExit("sigaction");

    /* Create the timer */
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN+1;
    sev.sigev_value.sival_ptr = &timerid;
    if (timer_create(CLOCK_THREAD_CPUTIME_ID, &sev, &timerid) == -1)
        errExit("timer_create");

    /* Start the timer */
    its.it_value.tv_sec = (PERIOD) / 1000000000;
    its.it_value.tv_nsec = (PERIOD) % 1000000000;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;

    if (timer_settime(timerid, 0, &its, NULL) == -1)
        errExit("timer_settime");
}


/*
 * This function handles a signal and invokes the specified interrupt
 * handler, ensuring that signals are unmasked first.
 */
void
handle_interrupt(int sig, siginfo_t *si, ucontext_t *ucontext)
{
    uint64_t eip = ucontext->uc_mcontext.gregs[RIP];
    unsigned long *gr;
	
	printf("here");

    /*
     * This allows us to check the interrupt level
     * and effectively block other signals.
     *
     * We can check the interrupt level and disable
     * them, which will prevent other signals from
     * interfering with our other check for library
     * calls.
     */
    if(interrupt_level==ENABLED &&
            eip > (uint64_t)start &&
            eip < (uint64_t)end){

        unsigned long *newsp, *newfp;
        /*
         * push the return address
         */
        newsp = (unsigned long *) ucontext->uc_mcontext.gregs[RSP];
        newsp--;
        *newsp-- =(unsigned long)ucontext->uc_mcontext.gregs[RIP];

        /*
         * make room for saved state and align stack.
         */
#define ROUND(X,Y)   (((unsigned long)X) & ~(Y-1)) /* Y must be a power of 2 */
        newsp = (unsigned long *) ROUND(newsp, 16);
       
        //memcpy not async signal safe, so just copy piece by piece
        gr = (unsigned long *)ucontext->uc_mcontext.gregs;
        *newsp-- = 0xdeadbeefcafebabe; //shouldn't ever use this space, use for debug.
        *newsp-- = gr[RSP];
        newfp = newsp;
        *newsp-- = gr[RBP];
        *newsp-- = gr[RDX];
        *newsp-- = gr[RCX];
        *newsp-- = gr[RBX];
        *newsp-- = gr[RAX];
        *newsp-- = gr[RSI];
        *newsp-- = gr[RDI];
        *newsp-- = gr[R15];
        *newsp-- = gr[R14];
        *newsp-- = gr[R13];
        *newsp-- = gr[R12];
        *newsp-- = gr[R11];
        *newsp-- = gr[R10];
        *newsp-- = gr[R9];
        *newsp-- = gr[R8];
        *newsp = (unsigned long)minithread_trampoline;  /* return address */

        /*
         * set the context so that we end up in the student's clock handler
         * and our stack pointer is at the return address we just pushed onto
         * the stack.
         *
         * RBP should be set above where we push the base pointer.
         */
        ucontext->uc_mcontext.gregs[RSP]=(unsigned long)newsp; 
        ucontext->uc_mcontext.gregs[RBP]=(unsigned long)newfp; 
        ucontext->uc_mcontext.gregs[RIP]=(unsigned long)mini_clock_handler;
        ucontext->uc_mcontext.gregs[RDI]=(unsigned long)0;
        
    }
}

