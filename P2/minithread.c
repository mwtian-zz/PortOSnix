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

/*
 * A minithread should be defined either in this file or in a private
 * header file.  Minithreads have a stack pointer with to make procedure
 * calls, a stackbase which points to the bottom of the procedure
 * call stack, the ability to be enqueueed and dequeued, and any other state
 * that you feel they must have.
 */

/*
 * struct minithread is defined in the private header "minithread_private.h".
 */

/* File scope variables */
static struct minithread _idle_thread_;
static minithread_t const idle_thread = &_idle_thread_;
static struct thread_monitor thread_monitor;

/* File scope functions */
static void minithread_schedule();
static minithread_t minithread_pickold();
static minithread_t minithread_picknew();
static int minithread_exit(arg_t arg);
static void minithread_cleanup();
static int minithread_initialize_thread_monitor();
static int minithread_initialize_idle();
static int minithread_initialize_clock();
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
    t->id = thread_monitor.tidcount;
    ++(thread_monitor.tidcount);
    ++(thread_monitor.count);
    set_interrupt_level(oldlevel);

    return t;
}

/*
 * Release memory of exited threads.
 */
static void
minithread_cleanup()
{
    minithread_t t;
    interrupt_level_t oldlevel = set_interrupt_level(DISABLED);
    while (0 == queue_dequeue(thread_monitor.exited, (void**)&t)
            && NULL != t) {
        if (NULL != t->base)
            minithread_free_stack(t->base);
        free(t);
        --(thread_monitor.count);
    }
    set_interrupt_level(oldlevel);
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
    multilevel_queue_enqueue(thread_monitor.ready, t->priority, t);
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
    thread_monitor.instack->status = BLOCKED;
    minithread_schedule();
    set_interrupt_level(oldlevel);
}

/*
 * This is the 'final_proc' that helps threads exit properly.
 */
static int
minithread_exit(arg_t arg)
{
    set_interrupt_level(DISABLED);
    thread_monitor.instack->status = EXITED;
    queue_append(thread_monitor.exited, thread_monitor.instack);
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
    minithread_t t = thread_monitor.instack;
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
    minithread_t rt_old = minithread_pickold();
    minithread_t rt_new = minithread_picknew();
    if (NULL == rt_old || NULL == rt_new)
        return;
    thread_monitor.instack = rt_new;
    thread_monitor.expire =
    ticks + thread_monitor.quanta_lim[rt_new->priority];
    rt_new->status = RUNNING;
    /* Switch only when the threads are different. */
    if (rt_old != rt_new)
        minithread_switch(&(rt_old->top),&(rt_new->top));
}

/*
 * Return the pointer to the thread leaving stack.
 * Place the thread in the appropriate queue.
 */
static minithread_t
minithread_pickold()
{
    minithread_t t = thread_monitor.instack;
    /* Reduce privilige if runs out of quanta */
    if (ticks == thread_monitor.expire)
        if (t->priority < MAX_PRIORITY)
            ++t->priority;
    if (t != idle_thread && t->status == READY)
        if (-1 == multilevel_queue_enqueue(thread_monitor.ready, t->priority, t))
            return NULL;
    return t;
}


/*
 * Return the pointer to the thread entering stack.
 * Return idle_thread if there is no other thread to run.
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
    /* Switch to idle thread when the ready queue is empty. */
    for (i = 0; i <= MAX_PRIORITY; ++i) {
        if (0 == multilevel_queue_dequeue(thread_monitor.ready,
                                          (p + i) % (MAX_PRIORITY + 1),
                                          (void**) &t))
            return t;
    }

    return idle_thread;
}

/* Return pointer to the running thread. */
minithread_t
minithread_self()
{
    return thread_monitor.instack;
}

/* Return ID of the running thread. */
int
minithread_id()
{
    return thread_monitor.instack->id;
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
    if (-1 == minithread_initialize_idle()) {
        printf("Idle thread initialization failed.\n");
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

    while (1) {
        minithread_cleanup();
        minithread_yield();
    }
}

static int
minithread_initialize_thread_monitor()
{
    int i;
    thread_monitor.count = 1;
    thread_monitor.quanta_lim[0] = 1;
    for (i = 1; i <= MAX_PRIORITY; ++i)
        thread_monitor.quanta_lim[i] = 2 * thread_monitor.quanta_lim[i - 1];
    thread_monitor.instack = idle_thread;
    thread_monitor.ready = multilevel_queue_new(MAX_PRIORITY + 1);
    thread_monitor.exited = queue_new();
    if (NULL == thread_monitor.ready || NULL == thread_monitor.exited)
        return -1;
    return 0;
}

static int
minithread_initialize_idle()
{
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
    if (++ticks >= thread_monitor.expire)
        minithread_yield();
    set_interrupt_level(oldlevel);
}
