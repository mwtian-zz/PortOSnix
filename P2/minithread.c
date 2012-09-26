/*
 * minithread.c:
 *  This file provides a few function headers for the procedures that
 *  you are required to implement for the minithread assignment.
 *
 *  EXCEPT WHERE NOTED YOUR IMPLEMENTATION MUST CONFORM TO THE
 *  NAMING AND TYPING OF THESE PROCEDURES.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "interrupts.h"
#include "queue.h"
#include "synch.h"
#include "minithread.h"
#include "minithread_private.h"
#include "alarm.h"

/*
 * A minithread should be defined either in this file or in a private
 * header file.  Minithreads have a stack pointer with to make procedure
 * calls, a stackbase which points to the bottom of the procedure
 * call stack, the ability to be enqueueed and dequeued, and any other state
 * that you feel they must have.
 */

/*
 * Information for thread management.
 */
static unsigned int count;
static unsigned int tidcount;
static minithread_t instack;
static multilevel_queue_t ready;
static queue_t exited;
static long expire;
static int quanta_lim[MAX_PRIORITY + 1];
static semaphore_t thread_monitor_sem; /* thread monitor semaphore */
static semaphore_t exit_count;
static semaphore_t exit_muxtex;
static struct minithread _idle_thread_;
static minithread_t const idle_thread = &_idle_thread_;

/*
 * struct minithread is defined in the private header "minithread_private.h".
 */

/* File scope functions */
static void minithread_schedule();
static minithread_t minithread_pickold();
static minithread_t minithread_picknew();
static int minithread_exit(arg_t arg);
static int minithread_cleanup();
static int minithread_initialize_thread_monitor();
static int minithread_initialize_systhreads();
static int minithread_initialize_clock();
static int minithread_initialize_sem();
static void clock_handler();

/* minithread functions */

/*
 * Allocate memory and initialize a thread.
 * Return NULL when allocation fails.
 */
minithread_t
minithread_create(proc_t proc, arg_t arg)
{
    minithread_t t;
    interrupt_level_t oldlevel;
    /* Allocate memory for TCB and stack. */
    if ((t = malloc(sizeof(*t))) == NULL) {
        printf("TCB memory allocation failed.\n");
        return NULL;
    }
    minithread_allocate_stack(&(t->base), &(t->top));
    if (NULL == t->base || NULL == t->top) {
        printf("Stack allocation failed.\n");
        free(t);
        return NULL;
    }
    /* Initialize TCB and thread stack. */
    minithread_initialize_stack(&(t->top), proc, arg, minithread_exit, NULL);
    t->qnode.prev = NULL;
    t->qnode.next = NULL;
    t->status = INITIAL;
    t->priority = 0;

    oldlevel = set_interrupt_level(DISABLED);
    t->id = tidcount;
    ++tidcount;
    ++count;
    set_interrupt_level(oldlevel);

    return t;
}

/*
 * Release memory of exited threads.
 */
static int
minithread_cleanup(arg_t arg)
{
    minithread_t t;
    while (1) {
        semaphore_P(exit_count);
        semaphore_P(exit_muxtex);
        queue_dequeue(exited, (void**) &t);
        semaphore_V(exit_muxtex);
        if (NULL != t) {
            if (NULL != t->base)
                minithread_free_stack(t->base);

            free(t);
        }
        --(count);
        printf("%d\n", count);
    }
    return 0;
}

/* Add thread t to the end of the appropriate ready queue. */
void
minithread_start(minithread_t t)
{
    interrupt_level_t oldlevel;
    if (NULL == t)
        return;
    oldlevel = set_interrupt_level(DISABLED);
    t->status = READY;
    multilevel_queue_enqueue(ready, t->priority, t);
    set_interrupt_level(oldlevel);
}

/*
 * The calling thread should be placed on the appropriate wait queue
 * before calling minithread_stop().
 */
void
minithread_stop()
{
    interrupt_level_t oldlevel = set_interrupt_level(DISABLED);
    instack->status = BLOCKED;
    minithread_schedule();
    set_interrupt_level(oldlevel);
}

/*
 * This is the 'final_proc' that helps threads exit properly.
 */
static int
minithread_exit(arg_t arg)
{
    semaphore_P(exit_muxtex);
    instack->status = EXITED;
    queue_append(exited, instack);
    semaphore_V(exit_muxtex);
    semaphore_V(exit_count);
    minithread_schedule();
    /*
     * The thread is switched out before this step,
     * so this thread is not going to return.
     * The return statement is just to make the compiler happy.
     */
    return 0;
}

/*
 * Add the running thread to the tail of the appropriate ready queue.
 * Then schedule for next thread.
 */
void
minithread_yield()
{
    minithread_t t = instack;
    interrupt_level_t oldlevel = set_interrupt_level(DISABLED);
    t->status = READY;
    minithread_schedule();
    set_interrupt_level(oldlevel);
}

/* Create and start a thread. */
minithread_t
minithread_fork(proc_t proc, arg_t arg)
{
    minithread_t t = minithread_create(proc, arg);
    if (NULL == t)
        return NULL;
    minithread_start(t);
    return t;
}

/*
 * The running thread should be placed on the correct queue
 * before calling the scheduler.
 */
static void
minithread_schedule()
{
    minithread_t rt_old;
    minithread_t rt_new;
    if (NULL == (rt_old = minithread_pickold())
        || NULL == (rt_new = minithread_picknew()))
        return;
    /* Switch only when the threads are different. */
    if (rt_old != rt_new)
        minithread_switch(&(rt_old->top),&(rt_new->top));
}

/*
 * Return the pointer to the thread leaving stack.
 * Place leaving thread in the appropriate queue and adjust its priority.
 */
static minithread_t
minithread_pickold()
{
    minithread_t t = instack;
    /* Reduce privilige if runs out of quanta */
    if (ticks == expire)
        if (t->priority < MAX_PRIORITY)
            ++t->priority;
    if (t != idle_thread && t->status == READY)
        if (-1 == multilevel_queue_enqueue(ready, t->priority, t))
            return NULL;
    return t;
}


/*
 * Return the pointer to the thread entering stack.
 * Return idle_thread if there is no other thread to run.
 * Set up the new thread with its expiration time, status and instack pointer.
 */
static minithread_t
minithread_picknew()
{
    minithread_t t;
    int i;
    int p;
    int r = ticks % 160;
    if (r < 80)
        p = 0;
    else if (r < 120)
        p = 1;
    else if (r < 144)
        p = 2;
    else
        p = 3;
    expire = ticks + quanta_lim[p];
    /* Switch to idle thread when the ready queue is empty. */
    for (i = 0; i <= MAX_PRIORITY; ++i) {
        if (0 == multilevel_queue_dequeue(ready,
                                          (p + i) % (MAX_PRIORITY + 1),
                                          (void**) &t)) {
            instack = t;
            t->status = RUNNING;
            return t;
        }
    }
    return idle_thread;
}

/* Return pointer to the running thread. */
minithread_t
minithread_self()
{
    return instack;
}

/* Return ID of the running thread. */
int
minithread_id()
{
    return instack->id;
}

/*
 * Initialization.
 *
 *     minithread_system_initialize:
 *     This procedure should be called from your C main procedure
 *     to turn a single threaded UNIX process into a multithreaded
 *     program.
 *
 *     Initialize any private data structures.
 *      Create the idle thread.
 *       Fork the thread which should call mainproc(mainarg)
 *      Start scheduling.
 *
 */
void
minithread_system_initialize(proc_t mainproc, arg_t mainarg)
{
    if (-1 == minithread_initialize_thread_monitor()) {
        printf("Schedule/Exit queue creation failed.\n");
        exit(-1);
    }
    if (-1 == minithread_initialize_systhreads()) {
        printf("System threads initialization failed.\n");
        exit(-1);
    }
    if (NULL == minithread_fork(mainproc, mainarg)) {
        printf("Main thread initialization failed.\n");
        exit(-1);
    }
    if (-1 == minithread_initialize_clock()) {
        printf("Clock initialization failed.\n");
        exit(-1);
    }

	if (minithread_initialize_sem() == -1) {
		fprintf(stderr, "Semaphore initialization failed.\n");
		exit(-1);
	}

    while (1) {
        minithread_yield();
    }
}

static int
minithread_initialize_thread_monitor()
{
    int i;
    count = 1;
    quanta_lim[0] = 1;
    for (i = 1; i <= MAX_PRIORITY; ++i)
        quanta_lim[i] = 2 * quanta_lim[i - 1];
    instack = idle_thread;
    ready = multilevel_queue_new(MAX_PRIORITY + 1);
    exited = queue_new();
    if (NULL == ready || NULL == exited)
        return -1;
    return 0;
}

static int
minithread_initialize_systhreads()
{
    if (NULL == minithread_fork(minithread_cleanup, NULL))
        return -1;
    if (NULL == idle_thread)
        return -1;
    idle_thread->status = RUNNING;
    idle_thread->priority = MAX_PRIORITY;

    return 0;
}

static int
minithread_initialize_clock()
{
    ticks = 0;
    set_interrupt_level(ENABLED);
    minithread_clock_init(clock_handler);
    return 0;
}

/* Initialize semaphores */
static int
minithread_initialize_sem() {
	alarm_id_sem = semaphore_create();
	semaphore_initialize(alarm_id_sem, 1);
	thread_monitor_sem = semaphore_create();
	semaphore_initialize(thread_monitor_sem, 1);
	exit_count = semaphore_create();
	semaphore_initialize(exit_count, 0);
    exit_muxtex = semaphore_create();
    semaphore_initialize(exit_muxtex, 1);
	return 0;
}


/*
 * minithread_unlock_and_stop(tas_lock_t* lock)
 *	Atomically release the specified test-and-set lock and
 *	block the calling thread.
 */
void
minithread_unlock_and_stop(tas_lock_t* lock)
{
    interrupt_level_t oldlevel = set_interrupt_level(DISABLED);
    atomic_clear(lock);
    minithread_stop();
    set_interrupt_level(oldlevel);
}

/*
 * sleep with timeout in milliseconds
 */
void
minithread_sleep_with_timeout(int delay)
{

}

/*
 * This is the clock interrupt handling routine.
 * You have to call minithread_clock_init with this
 * function as parameter in minithread_system_initialize
 */
void
clock_handler(void* arg)
{
    interrupt_level_t oldlevel = set_interrupt_level(DISABLED);
    if (++ticks >= expire)
        minithread_yield();
    set_interrupt_level(oldlevel);
}
