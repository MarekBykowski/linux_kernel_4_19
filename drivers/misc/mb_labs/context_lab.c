// SPDX-License-Identifier: GPL-2.0
/*
 * context lab: run one reporting function from each kernel execution
 * context and print the preempt_count meter that tells them apart.
 *
 *   process   - module init, directly. The only sleepable context.
 *   proc+spin - still process context, but a spinlock has bumped the
 *               preempt-disable count: in_task() is still 1 yet
 *               in_atomic() is now true (why msleep() under a spinlock
 *               trips DEBUG_ATOMIC_SLEEP).
 *   softirq   - a tasklet, which runs in softirq context.
 *   hardirq   - an hrtimer callback, which on a non-RT kernel fires in
 *               hard interrupt context.
 *
 * NMI is the fourth context but is not reachable from a module on this
 * 4.19 arm64 kernel: there is no exported API to run arbitrary code in
 * NMI context, and arm64 only has NMIs at all with pseudo-NMI support
 * (request_nmi(), v5.1+, not present here). report_context() would
 * print in_nmi=1 if it ever did run there.
 */
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/preempt.h>
#include <linux/sched.h>

static void report_context(const char *where)
{
	pr_info("context_lab: %-9s pc=%08x  in_task=%d in_softirq=%d in_irq=%d in_nmi=%d  in_atomic=%d  cpu=%d\n",
		where, preempt_count(),
		!!in_task(), !!in_softirq(), !!in_irq(), !!in_nmi(),
		!!in_atomic(), raw_smp_processor_id());
}

static DEFINE_SPINLOCK(demo_lock);

static struct hrtimer ctx_timer;
static DECLARE_COMPLETION(softirq_done);
static DECLARE_COMPLETION(hardirq_done);

static void ctx_tasklet_fn(unsigned long data)
{
	report_context("softirq");
	complete(&softirq_done);
}
static DECLARE_TASKLET(ctx_tasklet, ctx_tasklet_fn, 0);

static enum hrtimer_restart ctx_timer_fn(struct hrtimer *t)
{
	report_context("hardirq");
	complete(&hardirq_done);
	return HRTIMER_NORESTART;
}

static int __init context_lab_init(void)
{
	unsigned long flags;

	pr_info("context_lab: reporting from each kernel execution context\n");

	/* 1. plain process context */
	report_context("process");

	/* 2. process context, made atomic by a spinlock */
	spin_lock_irqsave(&demo_lock, flags);
	report_context("proc+spin");
	spin_unlock_irqrestore(&demo_lock, flags);

	/* 3. softirq context via a tasklet */
	tasklet_schedule(&ctx_tasklet);
	wait_for_completion(&softirq_done);

	/* 4. hardirq context via an hrtimer (hard context on non-RT) */
	hrtimer_init(&ctx_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ctx_timer.function = ctx_timer_fn;
	hrtimer_start(&ctx_timer, ms_to_ktime(10), HRTIMER_MODE_REL);
	wait_for_completion(&hardirq_done);

	pr_info("context_lab: NMI not triggered (no module API on 4.19 arm64) — see source\n");
	return 0;
}

static void __exit context_lab_exit(void)
{
	hrtimer_cancel(&ctx_timer);
	tasklet_kill(&ctx_tasklet);
	pr_info("context_lab: unloaded\n");
}

module_init(context_lab_init);
module_exit(context_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("report the kernel execution context from process, softirq and hardirq");
