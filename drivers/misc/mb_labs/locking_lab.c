// SPDX-License-Identifier: GPL-2.0
/*
 * locking lab: the same producer pattern protected by a spinlock and
 * by a mutex. Two kthreads per lock hammer a shared counter; both
 * counters must end up exact, unlike the unprotected counter in the
 * atomics lab.
 *
 * Load with sleep_in_atomic=1 to msleep() inside the spinlock:
 * CONFIG_DEBUG_ATOMIC_SLEEP then prints
 * "BUG: sleeping function called from invalid context". Sleeping under
 * a mutex is legal and demonstrated unconditionally.
 */
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/completion.h>

#define LOOPS 100000

static bool sleep_in_atomic;
module_param(sleep_in_atomic, bool, 0444);
MODULE_PARM_DESC(sleep_in_atomic, "msleep under the spinlock to trigger DEBUG_ATOMIC_SLEEP");

static DEFINE_SPINLOCK(counter_lock);
static DEFINE_MUTEX(counter_mutex);
static long spin_counter;
static long mutex_counter;

static DECLARE_COMPLETION(spin_done_a);
static DECLARE_COMPLETION(spin_done_b);
static DECLARE_COMPLETION(mutex_done_a);
static DECLARE_COMPLETION(mutex_done_b);

static int spin_thread(void *arg)
{
	struct completion *done = arg;
	int i;

	for (i = 0; i < LOOPS; i++) {
		spin_lock(&counter_lock);
		spin_counter++;
		if (sleep_in_atomic && i == 0)
			msleep(1);	/* the bug DEBUG_ATOMIC_SLEEP exists to catch */
		spin_unlock(&counter_lock);
	}

	complete(done);
	return 0;
}

static int mutex_thread(void *arg)
{
	struct completion *done = arg;
	int i;

	for (i = 0; i < LOOPS; i++) {
		mutex_lock(&counter_mutex);
		mutex_counter++;
		if (i == 0)
			msleep(1);	/* legal: mutex owners may sleep */
		mutex_unlock(&counter_mutex);
	}

	complete(done);
	return 0;
}

static int __init locking_lab_init(void)
{
	pr_info("locking_lab: 2 threads x %d increments per lock%s\n",
		LOOPS, sleep_in_atomic ? " (sleep_in_atomic on)" : "");

	kthread_run(spin_thread, &spin_done_a, "mb_spin_a");
	kthread_run(spin_thread, &spin_done_b, "mb_spin_b");
	kthread_run(mutex_thread, &mutex_done_a, "mb_mutex_a");
	kthread_run(mutex_thread, &mutex_done_b, "mb_mutex_b");

	wait_for_completion(&spin_done_a);
	wait_for_completion(&spin_done_b);
	wait_for_completion(&mutex_done_a);
	wait_for_completion(&mutex_done_b);

	pr_info("locking_lab: spin_counter=%ld mutex_counter=%ld (expected %d each)\n",
		spin_counter, mutex_counter, 2 * LOOPS);

	return 0;
}

static void __exit locking_lab_exit(void)
{
	pr_info("locking_lab: unloaded\n");
}

module_init(locking_lab_init);
module_exit(locking_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("spinlock vs mutex contention example");
