// SPDX-License-Identifier: GPL-2.0
/*
 * workqueue lab: defer work to process context on a dedicated
 * workqueue. The work_struct is embedded in an object and recovered
 * in the handler with container_of().
 */
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>

struct my_object {
	int value;
	struct work_struct work;
};

static struct workqueue_struct *my_wq;
static struct my_object *global_obj;

static void my_work_handler(struct work_struct *work)
{
	struct my_object *obj = container_of(work, struct my_object, work);

	pr_info("workqueue_lab: work running in PID %d (%s), value=%d\n",
		current->pid, current->comm, obj->value);

	msleep(1000);

	pr_info("workqueue_lab: work finished\n");
}

static int __init workqueue_lab_init(void)
{
	pr_info("workqueue_lab: init\n");

	my_wq = alloc_workqueue("mb_lab_wq", WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!my_wq)
		return -ENOMEM;

	global_obj = kmalloc(sizeof(*global_obj), GFP_KERNEL);
	if (!global_obj) {
		destroy_workqueue(my_wq);
		return -ENOMEM;
	}

	global_obj->value = 555;

	INIT_WORK(&global_obj->work, my_work_handler);
	queue_work(my_wq, &global_obj->work);

	return 0;
}

static void __exit workqueue_lab_exit(void)
{
	pr_info("workqueue_lab: exit\n");

	cancel_work_sync(&global_obj->work);
	kfree(global_obj);
	destroy_workqueue(my_wq);
}

module_init(workqueue_lab_init);
module_exit(workqueue_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("queue_work example with embedded struct");
