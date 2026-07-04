// SPDX-License-Identifier: GPL-2.0
/*
 * lockdep lab: build a circular lock dependency (A->B then B->A) in
 * a single thread. Nothing ever deadlocks here, yet with
 * CONFIG_PROVE_LOCKING lockdep records the A->B ordering and prints a
 * "possible circular locking dependency detected" splat the moment
 * the B->A ordering is attempted — proving the deadlock *could*
 * happen if two CPUs raced the two orderings.
 */
#include <linux/module.h>
#include <linux/mutex.h>

static DEFINE_MUTEX(lock_a);
static DEFINE_MUTEX(lock_b);

static int __init lockdep_lab_init(void)
{
	pr_info("lockdep_lab: taking A then B\n");
	mutex_lock(&lock_a);
	mutex_lock(&lock_b);
	mutex_unlock(&lock_b);
	mutex_unlock(&lock_a);

	pr_info("lockdep_lab: taking B then A — expect a lockdep splat\n");
	mutex_lock(&lock_b);
	mutex_lock(&lock_a);
	mutex_unlock(&lock_a);
	mutex_unlock(&lock_b);

	pr_info("lockdep_lab: done, no deadlock occurred — lockdep warns on the *possibility*\n");
	return 0;
}

static void __exit lockdep_lab_exit(void)
{
	pr_info("lockdep_lab: unloaded\n");
}

module_init(lockdep_lab_init);
module_exit(lockdep_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("ABBA lock ordering caught by CONFIG_PROVE_LOCKING");
