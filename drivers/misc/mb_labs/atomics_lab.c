// SPDX-License-Identifier: GPL-2.0
/*
 * atomics lab: two kthreads increment a plain int and an atomic_t the
 * same number of times. plain++ compiles to load/add/store, so two
 * CPUs interleave and lose updates; atomic_inc() uses ldxr/stxr (or
 * LSE stadd) and never loses one.
 */
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/atomic.h>
#include <linux/completion.h>

#define LOOPS 1000000

static int plain_counter;
static atomic_t atomic_counter = ATOMIC_INIT(0);

static DECLARE_COMPLETION(done_a);
static DECLARE_COMPLETION(done_b);

static int inc_thread(void *arg)
{
	struct completion *done = arg;
	int i;

	for (i = 0; i < LOOPS; i++) {
		/*
		 * READ_ONCE/WRITE_ONCE keep the compiler from caching the
		 * counter in a register, but the load-add-store is still
		 * not atomic — updates from the other CPU get overwritten.
		 */
		WRITE_ONCE(plain_counter, READ_ONCE(plain_counter) + 1);
		atomic_inc(&atomic_counter);
	}

	complete(done);
	return 0;
}

static int __init atomics_lab_init(void)
{
	pr_info("atomics_lab: 2 threads x %d increments\n", LOOPS);

	kthread_run(inc_thread, &done_a, "mb_atomics_a");
	kthread_run(inc_thread, &done_b, "mb_atomics_b");

	wait_for_completion(&done_a);
	wait_for_completion(&done_b);

	pr_info("atomics_lab: plain=%d atomic=%d (expected %d) — plain lost %d updates\n",
		plain_counter, atomic_read(&atomic_counter), 2 * LOOPS,
		2 * LOOPS - plain_counter);

	return 0;
}

static void __exit atomics_lab_exit(void)
{
	pr_info("atomics_lab: unloaded\n");
}

module_init(atomics_lab_init);
module_exit(atomics_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("lost updates with a plain int vs atomic_t");
