// SPDX-License-Identifier: GPL-2.0
/*
 * rcu lab: a reader kthread dereferences a shared pointer locklessly
 * under rcu_read_lock() while a writer kthread swaps it with
 * rcu_assign_pointer() and only frees the old value after
 * synchronize_rcu() guarantees no reader can still see it.
 *
 * Both threads run until rmmod (kthread_stop), so watch the interleave
 * with dmesg -w while the module is loaded.
 */
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/delay.h>

static int __rcu *gptr;

static struct task_struct *reader_task;
static struct task_struct *writer_task;

static int reader_thread(void *arg)
{
	while (!kthread_should_stop()) {
		int *p, v;

		rcu_read_lock();
		p = rcu_dereference(gptr);
		v = p ? *p : -1;
		rcu_read_unlock();

		pr_info("rcu_lab: reader sees %d\n", v);
		msleep(200);
	}
	return 0;
}

static int writer_thread(void *arg)
{
	int i = 0;

	while (!kthread_should_stop()) {
		int *new, *old;

		new = kmalloc(sizeof(*new), GFP_KERNEL);
		if (!new)
			break;
		*new = ++i;

		/* sole writer, no lock needed: hence the "1" */
		old = rcu_dereference_protected(gptr, 1);
		rcu_assign_pointer(gptr, new);

		synchronize_rcu();
		kfree(old);

		pr_info("rcu_lab: writer published %d\n", i);
		msleep(500);
	}
	return 0;
}

static int __init rcu_lab_init(void)
{
	reader_task = kthread_run(reader_thread, NULL, "rcu_lab_reader");
	if (IS_ERR(reader_task))
		return PTR_ERR(reader_task);

	writer_task = kthread_run(writer_thread, NULL, "rcu_lab_writer");
	if (IS_ERR(writer_task)) {
		kthread_stop(reader_task);
		return PTR_ERR(writer_task);
	}

	return 0;
}

static void __exit rcu_lab_exit(void)
{
	int *old;

	kthread_stop(writer_task);
	kthread_stop(reader_task);

	old = rcu_dereference_protected(gptr, 1);
	RCU_INIT_POINTER(gptr, NULL);
	synchronize_rcu();
	kfree(old);

	pr_info("rcu_lab: unloaded\n");
}

module_init(rcu_lab_init);
module_exit(rcu_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("RCU reader/writer lab");
