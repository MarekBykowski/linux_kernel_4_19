// SPDX-License-Identifier: GPL-2.0
/*
 * waitqueue lab: consumer sleeps in wait_event() until the producer
 * sets the condition (first!) and then calls wake_up().
 * Init waits for both threads, so insmod stalls for ~1s.
 */
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/delay.h>

static DECLARE_WAIT_QUEUE_HEAD(my_wq);
static int data_ready;
static DECLARE_COMPLETION(consumer_done);

static int consumer_thread(void *arg)
{
	pr_info("waitqueue_lab: consumer waiting...\n");

	wait_event(my_wq, data_ready != 0);

	pr_info("waitqueue_lab: consumer woken up!\n");
	complete(&consumer_done);
	return 0;
}

static int producer_thread(void *arg)
{
	msleep(1000);

	pr_info("waitqueue_lab: producer setting data_ready\n");

	/* Important: condition first ... */
	WRITE_ONCE(data_ready, 1);
	/* ... then wake */
	wake_up(&my_wq);

	return 0;
}

static int __init waitqueue_lab_init(void)
{
	data_ready = 0;

	kthread_run(consumer_thread, NULL, "wq_lab_consumer");
	kthread_run(producer_thread, NULL, "wq_lab_producer");

	/* don't let rmmod pull the code out from under the threads */
	wait_for_completion(&consumer_done);
	return 0;
}

static void __exit waitqueue_lab_exit(void)
{
	pr_info("waitqueue_lab: unloaded\n");
}

module_init(waitqueue_lab_init);
module_exit(waitqueue_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("Wait queue producer/consumer lab");
