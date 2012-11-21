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

#include "alarm.h"
#include "interrupts.h"
#include "miniroute.h"
#include "miniheader.h"
#include "minimsg.h"
#include "minisocket.h"
#include "network.h"
#include "queue.h"
#include "read_private.h"
#include "synch.h"
#include "minithread.h"
#include "minithread_private.h"

/*
 * A minithread should be defined either in this file or in a private
 * header file.  Minithreads have a stack pointer with to make procedure
 * calls, a stackbase which points to the bottom of the procedure
 * call stack, the ability to be enqueueed and dequeued, and any other state
 * that you feel they must have.
 */

/*
 * File scope variables
 */

/* Number of threads */
static unsigned int thd_count;
/* Next thread id */
static unsigned int tid_count;
/* Pointer to the tcb of running thread */
static minithread_t context;
/* Pointer to the ready queue */
static multilevel_queue_t ready;
/* Pointer to the exited thread queue */
static queue_t exited;
/* Time when current running thread is scheduled to be switch out */
static long expire;
/* The quanta limit for each priority level */
static int quanta_lim[MAX_SCHED_PRIORITY + 1];
/* Semaphore for counting exited threads */
static semaphore_t exit_count;
/* Semaphore for atomically operating on the exited queue */
static semaphore_t exit_mutex;
/* Semaphore for atomically operating on thread counters */
static semaphore_t id_mutex;

/* Pointer to the idle thread */
static struct minithread _idle_thread_;
static minithread_t const idle_thread = &_idle_thread_;

/*
 * struct minithread is defined in the private header "minithread_private.h".
 */

/*
 * External variables used in this file
 */
extern long ticks;
extern long alarm_time;

/* File scope functions, explained below */
static void minithread_schedule();
static minithread_t minithread_picknew();
static int minithread_exit(arg_t arg);
static int minithread_cleanup();
static int minithread_initialize_scheduler();
static int minithread_initialize_sys_threads();
static int minithread_initialize_interrupts();
static int minithread_initialize_sys_sems();
static void clock_handler(void* arg);
static void network_handler(void* arg);

/* minithread functions */

/*
 * Allocate memory and initialize a thread.
 * Return NULL when allocation fails.
 */
minithread_t
minithread_create(proc_t proc, arg_t arg)
{
    minithread_t t;

    /* Allocate memory for TCB and stack. */
    if ((t = malloc(sizeof(struct minithread))) == NULL) {
        return NULL;
    }
    minithread_allocate_stack(&(t->base), &(t->top));
    if (NULL == t->base || NULL == t->top) {
        free(t);
        return NULL;
    }

    /* Initialize sleep semaphore */
    t->sleep_sem = semaphore_create();
    if (NULL == t->sleep_sem) {
        minithread_free_stack(t->base);
        free(t);
        return NULL;
    }
    semaphore_initialize(t->sleep_sem, 0);

    /* Initialize TCB and thread stack. */
    minithread_initialize_stack(&(t->top), proc, arg, minithread_exit, NULL);
    t->qnode.prev = NULL;
    t->qnode.next = NULL;
    t->status = INITIAL;
    t->priority = 0;

    semaphore_P(id_mutex);
    t->id = tid_count;
    ++tid_count;
    ++thd_count;
    semaphore_V(id_mutex);

    return t;
}

/*
 * Thread to release memory of exited threads.
 */
static int
minithread_cleanup(arg_t arg)
{
    minithread_t t = NULL;
    interrupt_level_t oldlevel;
    while (1) {
        oldlevel = set_interrupt_level(DISABLED);
        semaphore_P(exit_count);
        set_interrupt_level(oldlevel);
        semaphore_P(id_mutex);
        --thd_count;
        semaphore_V(id_mutex);
        semaphore_P(exit_mutex);
        queue_dequeue(exited, (void**) &t);
        if (t != NULL) {
            minithread_free_stack(t->base);
            semaphore_destroy(t->sleep_sem);
            free(t);
        }
        semaphore_V(exit_mutex);
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
 * DEPRECATED. Beginning from project 2, you should use
 * minithread_unlock_and_stop() instead of this function.
 */
void
minithread_stop()
{
    interrupt_level_t oldlevel = set_interrupt_level(DISABLED);
    context->status = BLOCKED;
    minithread_schedule();
    set_interrupt_level(oldlevel);
}

/*
 * This is the 'final_proc' that helps threads exit properly.
 */
static int
minithread_exit(arg_t arg)
{
    interrupt_level_t oldlevel;
    semaphore_P(exit_mutex);
    queue_append(exited, context);
    semaphore_V(exit_mutex);
    oldlevel = set_interrupt_level(DISABLED);
    semaphore_V(exit_count);
    context->status = EXITED;
    minithread_schedule();
    /*
     * The thread is switched out before this step,
     * so this thread is not going to return.
     * The return statement is just to make the compiler happy.
     */
    set_interrupt_level(oldlevel);
    return 0;
}

/*
 * Add the running thread to the tail of the appropriate ready queue.
 * Then schedule for next thread.
 */
void
minithread_yield()
{
    minithread_t t = context;
    interrupt_level_t oldlevel = set_interrupt_level(DISABLED);
    t->status = READY;
    /* Reduce its privilige if t runs out of quanta */
    if (ticks >= expire) {
        if (t->priority < MAX_SCHED_PRIORITY)
            ++t->priority;
    }
    if (t != idle_thread) {
        multilevel_queue_enqueue(ready, t->priority, t);
    }
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
 * Interrupts should be disabled before calling the scheduler.
 */
static void
minithread_schedule()
{
    minithread_t rt_old = context;
    minithread_t rt_new;
    /* Determine the next thread to run. NULL if there is no ready thread. */
    if ((rt_new = minithread_picknew()) == NULL)
        return;
    /* Switch only when the threads are different. */
    if (rt_old != rt_new)
        minithread_switch(&(rt_old->top),&(rt_new->top));
}

/*
 * Return the pointer to the next thread to run.
 * Return idle_thread if there is no other thread to run.
 * Set up the new thread with its expiration time, status and context pointer.
 */
static minithread_t
minithread_picknew()
{
    minithread_t t;
    int r = ticks % 160;
    int p;
    /* Determine how many quanta to assign to the next thread */
    if (r < 80)
        p = 0;
    else if (r < 120)
        p = 1;
    else if (r < 144)
        p = 2;
    else
        p = 3;
    /* Look for a ready thread, or switch to idle_thread */
    if (multilevel_queue_dequeue(ready, p, (void**) &t) > -1) {
        expire = ticks + quanta_lim[p];
    } else {
        t = idle_thread;
        expire = ticks + 1;
    }
    context = t;
    t->status = RUNNING;
    return t;
}

/* Return pointer to the running thread. */
minithread_t
minithread_self()
{
    return context;
}

/* Return ID of the running thread. */
int
minithread_id()
{
    return context->id;
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
 *      Fork the thread which should call mainproc(mainarg)
 *      Start scheduling.
 *
 */
void
minithread_system_initialize(proc_t mainproc, arg_t mainarg)
{
    if (minithread_initialize_scheduler() == -1) {
        exit(-1);
    }
    if (minithread_initialize_sys_sems() == -1) {
        exit(-1);
    }
    if (minithread_initialize_sys_threads() == -1) {
        exit(-1);
    }
    if (minithread_fork(mainproc, mainarg) == NULL) {
        exit(-1);
    }
    if (minithread_initialize_interrupts() == -1) {
        exit(-1);
    }

    minithread_yield();

    /* Idle loop */
    while (1)
        ;
}

static int
minithread_initialize_scheduler()
{
    int i;
    thd_count = 1;
    tid_count = 1;
    quanta_lim[0] = 1;
    for (i = 1; i <= MAX_SCHED_PRIORITY; ++i)
        quanta_lim[i] = 2 * quanta_lim[i - 1];
    ready = multilevel_queue_new(MAX_SCHED_PRIORITY + 1);
    exited = queue_new();
    if (NULL == ready || NULL == exited)
        return -1;
    context = idle_thread;
    return 0;
}

static int
minithread_initialize_sys_threads()
{
    if (NULL == minithread_fork(minithread_cleanup, NULL))
        return -1;
    if (NULL == idle_thread)
        return -1;
    idle_thread->id = 0;
    idle_thread->status = RUNNING;
    idle_thread->priority = MAX_SCHED_PRIORITY;
    if ((idle_thread->sleep_sem = semaphore_create()) == NULL)
        return -1;
    return 0;
}

static int
minithread_initialize_sys_sems()
{
    exit_count = semaphore_create();
    exit_mutex = semaphore_create();
    id_mutex = semaphore_create();
    if (NULL == exit_count || NULL == exit_mutex || NULL == id_mutex)
        return -1;
    semaphore_initialize(exit_count, 0);
    semaphore_initialize(exit_mutex, 1);
    semaphore_initialize(id_mutex, 1);
    return 0;
}

static int
minithread_initialize_interrupts()
{
    ticks = 0;
    expire = -1;
    set_interrupt_level(DISABLED);
    if (alarm_initialize() == -1)
        return -1;
    minithread_clock_init(clock_handler);
    network_initialize(network_handler);
    miniroute_initialize();
    minimsg_initialize();
    minisocket_initialize();
    miniterm_initialize();
    set_interrupt_level(ENABLED);
    return 0;
}

/*
 * minithread_unlock_and_stop(tas_lock_t* lock)
 *  Atomically release the specified test-and-set lock and
 *  block the calling thread.
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
 * Sleep with timeout in milliseconds
 */
void
minithread_sleep_with_timeout(int delay)
{
    interrupt_level_t oldlevel = set_interrupt_level(DISABLED);
    context->status = BLOCKED;
    if (register_alarm(delay, &semaphore_Signal, context->sleep_sem) != -1)
        semaphore_P(context->sleep_sem);
    set_interrupt_level(oldlevel);
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
    ticks++;
    if (alarm_time > -1 && ticks >= alarm_time)
        alarm_signal();
    if (ticks >= expire)
        minithread_yield();
    set_interrupt_level(oldlevel);
}

/* Interrupts are disabled in the respective processing functions */
void
network_handler(void* arg)
{
    interrupt_level_t oldlevel = set_interrupt_level(DISABLED);
    network_interrupt_arg_t *intrpt = arg;
    miniroute_buffer_intrpt(intrpt);
    set_interrupt_level(oldlevel);
}
