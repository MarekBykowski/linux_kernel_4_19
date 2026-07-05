// SPDX-License-Identifier: GPL-2.0
/*
 * timer lab: the two kernel timer facilities and the contexts their
 * callbacks run in.
 *
 *   hrtimer     - high-resolution (nanosecond) timer. On a non-RT
 *                 kernel its callback fires in HARD interrupt context
 *                 (in_irq()). Periodic here, re-arming with
 *                 hrtimer_forward_now() and measuring per-expiry jitter.
 *   timer_list  - classic jiffy-granularity timer. Its callback fires
 *                 in SOFTIRQ context (the TIMER softirq, in_softirq()).
 *
 * Neither callback may sleep. The lab prints the context flags from
 * both so the hardirq-vs-softirq difference is visible.
 */
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/timer.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/completion.h>

#define PERIOD_MS	100
#define EXPIRIES	10

static struct hrtimer my_timer;
static ktime_t last_fire;
static int fired;
static DECLARE_COMPLETION(timer_done);

static struct timer_list my_tl;
static DECLARE_COMPLETION(tl_done);

static enum hrtimer_restart my_timer_fn(struct hrtimer *timer)
{
	ktime_t now = ktime_get();
	s64 delta_us = ktime_to_us(ktime_sub(now, last_fire));

	fired++;
	if (fired == 1)
		pr_info("hrtimer_lab: hrtimer callback context: in_irq=%d in_softirq=%d (hardirq)\n",
			!!in_irq(), !!in_softirq());
	pr_info("hrtimer_lab: expiry %2d, %lld us since last (programmed %d000), jitter %+lld us\n",
		fired, delta_us, PERIOD_MS, delta_us - PERIOD_MS * 1000);
	last_fire = now;

	if (fired >= EXPIRIES) {
		complete(&timer_done);
		return HRTIMER_NORESTART;
	}

	hrtimer_forward_now(timer, ms_to_ktime(PERIOD_MS));
	return HRTIMER_RESTART;
}

static void my_tl_fn(struct timer_list *t)
{
	pr_info("hrtimer_lab: timer_list callback context: in_irq=%d in_softirq=%d (softirq)\n",
		!!in_irq(), !!in_softirq());
	complete(&tl_done);
}

static int __init hrtimer_lab_init(void)
{
	pr_info("hrtimer_lab: arming %d ms periodic hrtimer for %d expiries\n",
		PERIOD_MS, EXPIRIES);

	hrtimer_init(&my_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	my_timer.function = my_timer_fn;

	last_fire = ktime_get();
	hrtimer_start(&my_timer, ms_to_ktime(PERIOD_MS), HRTIMER_MODE_REL);

	wait_for_completion(&timer_done);
	pr_info("hrtimer_lab: hrtimer done, now arming a classic timer_list\n");

	timer_setup(&my_tl, my_tl_fn, 0);
	mod_timer(&my_tl, jiffies + msecs_to_jiffies(PERIOD_MS));

	wait_for_completion(&tl_done);
	pr_info("hrtimer_lab: done\n");
	return 0;
}

static void __exit hrtimer_lab_exit(void)
{
	hrtimer_cancel(&my_timer);
	del_timer_sync(&my_tl);
	pr_info("hrtimer_lab: unloaded\n");
}

module_init(hrtimer_lab_init);
module_exit(hrtimer_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("hrtimer (hardirq) vs timer_list (softirq), with jitter");
