// SPDX-License-Identifier: GPL-2.0
/*
 * workqueue lab: defer work to process context two ways, to contrast
 * the frontend (which queue) with the backend (which worker runs it):
 *
 *   1. own workqueue    - alloc_workqueue() + queue_work(): a dedicated,
 *      named, WQ_UNBOUND queue. Runs on an UNBOUND worker pool, so the
 *      kworker is named "kworker/uNN:x" (the 'u' = unbound).
 *   2. system workqueue - schedule_work(): the shared system_wq. Runs on
 *      a per-CPU BOUND pool, so the kworker is "kworker/N:x".
 *
 * Same handler both times; compare current->comm to see the difference.
 * Since CMWQ a workqueue is a *queue with attributes*, not a thread —
 * both paths execute on kernel-managed shared kworkers, not a thread
 * the module created. The work_struct is embedded in an object and
 * recovered in the handler with container_of().
 */
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>

struct my_object {
	int value;
	const char *tag;
	struct work_struct work;
};

static struct workqueue_struct *my_wq;
static struct my_object *own_obj;	/* runs on our own unbound wq */
static struct my_object *sys_obj;	/* runs on the system wq */

static void my_work_handler(struct work_struct *work)
{
	struct my_object *obj = container_of(work, struct my_object, work);

	pr_info("workqueue_lab: [%s] running on %s (PID %d), value=%d\n",
		obj->tag, current->comm, current->pid, obj->value);

	msleep(300);

	pr_info("workqueue_lab: [%s] finished\n", obj->tag);
}

static int __init workqueue_lab_init(void)
{
	pr_info("workqueue_lab: init\n");

	/* 1. our own dedicated, unbound workqueue */
	my_wq = alloc_workqueue("mb_lab_wq", WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!my_wq)
		return -ENOMEM;

	own_obj = kmalloc(sizeof(*own_obj), GFP_KERNEL);
	if (!own_obj) {
		destroy_workqueue(my_wq);
		return -ENOMEM;
	}
	own_obj->value = 555;
	own_obj->tag = "own wq (unbound)";
	INIT_WORK(&own_obj->work, my_work_handler);
	queue_work(my_wq, &own_obj->work);

	/* 2. the shared system workqueue via schedule_work() */
	sys_obj = kmalloc(sizeof(*sys_obj), GFP_KERNEL);
	if (!sys_obj) {
		cancel_work_sync(&own_obj->work);
		kfree(own_obj);
		destroy_workqueue(my_wq);
		return -ENOMEM;
	}
	sys_obj->value = 777;
	sys_obj->tag = "system wq (bound)";
	INIT_WORK(&sys_obj->work, my_work_handler);
	schedule_work(&sys_obj->work);		/* == queue_work(system_wq, ...) */

	return 0;
}

static void __exit workqueue_lab_exit(void)
{
	pr_info("workqueue_lab: exit\n");

	cancel_work_sync(&own_obj->work);
	cancel_work_sync(&sys_obj->work);
	destroy_workqueue(my_wq);
	kfree(own_obj);
	kfree(sys_obj);
}

module_init(workqueue_lab_init);
module_exit(workqueue_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("queue_work on an own wq vs schedule_work on the system wq");
