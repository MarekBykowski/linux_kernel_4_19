// SPDX-License-Identifier: GPL-2.0
/*
 * findtask lab: locate task(s) by name. There is no name->task lookup
 * in the kernel, so we walk the task list and strcmp() each ->comm.
 *
 * Load with name=<comm> (default "ksoftirqd/0"). For each match it
 * prints pid, whether it is a kernel thread (PF_KTHREAD), its mm state
 * (NULL for kthreads) and its parent — kthreads are children of
 * kthreadd (pid 2).
 */
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>	/* for_each_process */

static char *name = "ksoftirqd/0";
module_param(name, charp, 0444);
MODULE_PARM_DESC(name, "task comm to search for");

static int __init findtask_lab_init(void)
{
	struct task_struct *t;
	int matches = 0;

	pr_info("findtask_lab: searching for comm=\"%s\"\n", name);

	rcu_read_lock();		/* tasks may exit under us; hold RCU */
	for_each_process(t) {
		if (strcmp(t->comm, name))
			continue;

		pr_info("findtask_lab: pid=%d tgid=%d comm=%-16s %s mm=%s parent=%d(%s) state=0x%lx\n",
			t->pid, t->tgid, t->comm,
			(t->flags & PF_KTHREAD) ? "[kthread]" : "[user]",
			t->mm ? "user" : "NULL",
			t->real_parent->pid, t->real_parent->comm,
			t->state);
		matches++;
	}
	rcu_read_unlock();

	pr_info("findtask_lab: %d match(es) for \"%s\"\n", matches, name);
	return 0;
}

static void __exit findtask_lab_exit(void)
{
	pr_info("findtask_lab: unloaded\n");
}

module_init(findtask_lab_init);
module_exit(findtask_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("find task_struct(s) by comm name");
