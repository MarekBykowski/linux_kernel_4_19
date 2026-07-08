// SPDX-License-Identifier: GPL-2.0
/*
 * locks lab: spinlock vs mutex vs semaphore, side by side.
 *
 * Three demonstrations:
 *
 * 1. Concurrency limit — 4 worker kthreads repeatedly enter a critical
 *    section. spinlock and mutex are mutual exclusion (max 1 holder);
 *    a counting semaphore initialised to 2 allows up to 2 at once.
 *
 * 2. Context — while holding each lock we print preempt_count/in_atomic:
 *    a spinlock disables preemption (atomic, must not sleep); a mutex
 *    and a semaphore are sleeping locks (preemptible, may sleep).
 *
 * 3. Ownership — a mutex has an owner: only the locker may unlock it.
 *    A semaphore has none: one thread can up() what another down()'d,
 *    so it doubles as a cross-thread signal (producer/consumer).
 */
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/completion.h>

#define NWORKERS 4
#define ITERS    4

enum lock_kind { L_SPIN, L_MUTEX, L_SEM };

static DEFINE_SPINLOCK(slock);
static DEFINE_MUTEX(mlock);
static struct semaphore sem;		/* counting, initialised to 2 */

static enum lock_kind kind;
static atomic_t active;			/* current holders */
static atomic_t max_seen;		/* peak concurrent holders */
static atomic_t remaining;		/* workers still running */
static struct completion round_done;

static void lock_take(void)
{
	switch (kind) {
	case L_SPIN:  spin_lock(&slock); break;
	case L_MUTEX: mutex_lock(&mlock); break;
	case L_SEM:   down(&sem); break;
	}
}

static void lock_give(void)
{
	switch (kind) {
	case L_SPIN:  spin_unlock(&slock); break;
	case L_MUTEX: mutex_unlock(&mlock); break;
	case L_SEM:   up(&sem); break;
	}
}

static void hold_delay(void)
{
	if (kind == L_SPIN)
		udelay(500);		/* atomic: busy-wait, must not sleep */
	else
		msleep(10);		/* sleeping lock: sleeping is legal */
}

static int worker(void *arg)
{
	int i, now;

	for (i = 0; i < ITERS; i++) {
		lock_take();
		now = atomic_inc_return(&active);
		if (now > atomic_read(&max_seen))
			atomic_set(&max_seen, now);	/* racy, fine for a demo */
		hold_delay();
		atomic_dec(&active);
		lock_give();
	}

	if (atomic_dec_and_test(&remaining))
		complete(&round_done);
	return 0;
}

static void run_round(enum lock_kind k, const char *name)
{
	int i;

	kind = k;
	atomic_set(&active, 0);
	atomic_set(&max_seen, 0);
	atomic_set(&remaining, NWORKERS);
	reinit_completion(&round_done);

	for (i = 0; i < NWORKERS; i++)
		kthread_run(worker, NULL, "lk_%s_%d", name, i);

	wait_for_completion(&round_done);
	pr_info("locks_lab: %-9s -> max %d concurrent holder(s) of %d workers\n",
		name, atomic_read(&max_seen), NWORKERS);
}

/* demo 2: what each lock does to preemption while held */
static void show_context(void)
{
	spin_lock(&slock);
	pr_info("locks_lab: spinlock  held: preempt_count=%08x in_atomic=%d (no sleep)\n",
		preempt_count(), !!in_atomic());
	spin_unlock(&slock);

	mutex_lock(&mlock);
	pr_info("locks_lab: mutex     held: preempt_count=%08x in_atomic=%d (may sleep)\n",
		preempt_count(), !!in_atomic());
	mutex_unlock(&mlock);

	down(&sem);
	pr_info("locks_lab: semaphore held: preempt_count=%08x in_atomic=%d (may sleep)\n",
		preempt_count(), !!in_atomic());
	up(&sem);
}

/* demo 3: a semaphore has no owner — released by a different thread */
static struct semaphore sig;
static struct completion sig_reported;

static int sig_waiter(void *arg)
{
	pr_info("locks_lab: [waiter] down(&sig) blocking, waiting for another thread to up()\n");
	down(&sig);				/* blocks until someone else up()s */
	pr_info("locks_lab: [waiter] woke — released by up() from a DIFFERENT thread\n");
	complete(&sig_reported);
	return 0;
}

static void show_ownership(void)
{
	sema_init(&sig, 0);			/* starts locked */
	init_completion(&sig_reported);

	kthread_run(sig_waiter, NULL, "lk_sig");
	msleep(50);				/* let the waiter block in down() */
	pr_info("locks_lab: [init] up(&sig) from this thread (the waiter never locked it)\n");
	up(&sig);				/* a mutex could NOT be unlocked like this */
	wait_for_completion(&sig_reported);
}

static int __init locks_lab_init(void)
{
	sema_init(&sem, 2);			/* counting semaphore: 2 permits */
	init_completion(&round_done);

	pr_info("locks_lab: 1) concurrency limit (semaphore permits = 2)\n");
	run_round(L_SPIN,  "spinlock");
	run_round(L_MUTEX, "mutex");
	run_round(L_SEM,   "semaphore");

	pr_info("locks_lab: 2) context while held\n");
	show_context();

	pr_info("locks_lab: 3) ownership (semaphore has none)\n");
	show_ownership();

	pr_info("locks_lab: done\n");
	return 0;
}

static void __exit locks_lab_exit(void)
{
	pr_info("locks_lab: unloaded\n");
}

module_init(locks_lab_init);
module_exit(locks_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("spinlock vs mutex vs semaphore: concurrency, context, ownership");
