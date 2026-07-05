// SPDX-License-Identifier: GPL-2.0
/*
 * current lab: who is `current`?
 *
 * `current` is a macro for the task_struct of whatever task is running
 * on this CPU. On arm64 get_current() reads it straight from the
 * SP_EL0 system register, kept up to date by the scheduler on every
 * context switch — so this lab prints both and checks they match.
 *
 * It reports `current` from three places to show the identity change:
 *   init      - runs in the context of the insmod/modprobe process:
 *               current is that user task, current->mm is its address
 *               space.
 *   kthread   - a kernel thread: current is the kthread itself, a
 *               different task_struct, and current->mm is NULL (it only
 *               borrows an active_mm).
 *   hardirq   - an hrtimer callback: current is simply whatever task
 *               was interrupted on this CPU, borrowed for the duration.
 */
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/completion.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/mm.h>

static struct task_struct *init_task_ctx;	/* who ran module init */

static DECLARE_COMPLETION(kthread_done);
static DECLARE_COMPLETION(hardirq_done);
static struct hrtimer cur_timer;

static void report_current(const char *where)
{
	struct task_struct *t = current;
	unsigned long sp_el0;

	asm volatile("mrs %0, sp_el0" : "=r" (sp_el0));

	pr_info("current_lab: %-8s current=%px sp_el0=%lx match=%d | pid=%d tgid=%d comm=%-16s mm=%s uid=%u\n",
		where, t, sp_el0, sp_el0 == (unsigned long)t,
		t->pid, t->tgid, t->comm,
		t->mm ? "user" : "NULL(kthread)",
		from_kuid(&init_user_ns, current_uid()));
}

static int current_kthread_fn(void *unused)
{
	report_current("kthread");
	pr_info("current_lab:          kthread current=%px vs init current=%px -> %s task; active_mm=%px\n",
		current, init_task_ctx,
		current == init_task_ctx ? "same" : "different",
		current->active_mm);
	complete(&kthread_done);
	return 0;
}

static enum hrtimer_restart current_timer_fn(struct hrtimer *t)
{
	report_current("hardirq");
	complete(&hardirq_done);
	return HRTIMER_NORESTART;
}

static int __init current_lab_init(void)
{
	init_task_ctx = current;

	pr_info("current_lab: on arm64 `current` is read from SP_EL0 (match=1 proves it)\n");

	/* process context: current is the modprobe/insmod task */
	report_current("init");

	/* kernel thread: its own task, no user mm */
	kthread_run(current_kthread_fn, NULL, "mb_current");
	wait_for_completion(&kthread_done);

	/* hardirq: current is the interrupted task, borrowed */
	hrtimer_init(&cur_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cur_timer.function = current_timer_fn;
	hrtimer_start(&cur_timer, ms_to_ktime(10), HRTIMER_MODE_REL);
	wait_for_completion(&hardirq_done);

	return 0;
}

static void __exit current_lab_exit(void)
{
	hrtimer_cancel(&cur_timer);
	pr_info("current_lab: unloaded\n");
}

module_init(current_lab_init);
module_exit(current_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("who is current, across process/kthread/hardirq, via SP_EL0");
