// SPDX-License-Identifier: GPL-2.0
/*
 * hrtimer lab: a periodic high-resolution timer. The callback runs in
 * hard interrupt context (so no sleeping there), re-arms itself with
 * hrtimer_forward_now() and measures how late each expiry fired
 * relative to the programmed period.
 */
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/completion.h>

#define PERIOD_MS	100
#define EXPIRIES	10

static struct hrtimer my_timer;
static ktime_t last_fire;
static int fired;
static DECLARE_COMPLETION(timer_done);

static enum hrtimer_restart my_timer_fn(struct hrtimer *timer)
{
	ktime_t now = ktime_get();
	s64 delta_us = ktime_to_us(ktime_sub(now, last_fire));

	fired++;
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

static int __init hrtimer_lab_init(void)
{
	pr_info("hrtimer_lab: arming %d ms periodic timer for %d expiries\n",
		PERIOD_MS, EXPIRIES);

	hrtimer_init(&my_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	my_timer.function = my_timer_fn;

	last_fire = ktime_get();
	hrtimer_start(&my_timer, ms_to_ktime(PERIOD_MS), HRTIMER_MODE_REL);

	wait_for_completion(&timer_done);
	pr_info("hrtimer_lab: done\n");
	return 0;
}

static void __exit hrtimer_lab_exit(void)
{
	hrtimer_cancel(&my_timer);
	pr_info("hrtimer_lab: unloaded\n");
}

module_init(hrtimer_lab_init);
module_exit(hrtimer_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("periodic hrtimer with per-expiry jitter measurement");
