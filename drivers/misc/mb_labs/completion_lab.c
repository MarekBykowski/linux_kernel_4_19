// SPDX-License-Identifier: GPL-2.0
/*
 * completion lab: block until a worker kthread signals completion.
 * Note insmod itself stalls for ~1s: init sleeps in
 * wait_for_completion() until the worker calls complete().
 */
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/delay.h>

static DECLARE_COMPLETION(my_comp);

static int worker_thread(void *data)
{
	msleep(1000);
	complete(&my_comp);
	return 0;
}

static int __init completion_lab_init(void)
{
	pr_info("completion_lab: starting worker, waiting for it\n");

	kthread_run(worker_thread, NULL, "completion_lab");
	wait_for_completion(&my_comp);

	pr_info("completion_lab: worker finished\n");
	return 0;
}

static void __exit completion_lab_exit(void)
{
	pr_info("completion_lab: unloaded\n");
}

module_init(completion_lab_init);
module_exit(completion_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("Completion synchronisation lab");
