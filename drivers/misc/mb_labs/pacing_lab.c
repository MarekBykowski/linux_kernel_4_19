// SPDX-License-Identifier: GPL-2.0
/*
 * pacing lab: hrtimer-driven packet pacing, the sch_fq way, with no NIC.
 *
 * A "packet" is just a sequence number. We pace <packets> of them at a
 * fixed <gap_us> spacing and measure the achieved inter-packet gap, to
 * show that an hrtimer delivers microsecond-accurate timing.
 *
 * The split mirrors net/sched/sch_fq.c:
 *   - pacing_timer_fn() fires in HARD interrupt context at each gap and
 *     does the one cheap thing it may: kick the bottom half. It does
 *     NOT "transmit" (that would be heavy/sleepable work in hardirq).
 *     Real sch_fq raises the NET_TX softirq via __netif_schedule(); a
 *     module can't own a softirq, so we use a tasklet, which is itself
 *     a client of the TASKLET softirq.
 *   - tx_tasklet_fn() runs in SOFTIRQ context and does the "transmit":
 *     it logs the packet, computes the next send time, and re-arms the
 *     hrtimer for the next gap using ABSOLUTE time so per-packet error
 *     does not accumulate into drift.
 */
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/completion.h>

static int gap_us = 500;
module_param(gap_us, int, 0444);
MODULE_PARM_DESC(gap_us, "inter-packet gap in microseconds");

static int packets = 10;
module_param(packets, int, 0444);
MODULE_PARM_DESC(packets, "number of packets to pace");

static struct hrtimer pacing_timer;
static ktime_t base;		/* t0: all sends are base + seq*gap */
static ktime_t last_tx;
static int seq;			/* next packet to send (1-based) */
static DECLARE_COMPLETION(pacing_done);

static void arm_next(void)
{
	/* absolute deadline for packet <seq>: base + seq*gap, no drift */
	ktime_t when = ktime_add_us(base, (u64)seq * gap_us);

	hrtimer_start(&pacing_timer, when, HRTIMER_MODE_ABS);
}

/* bottom half: softirq context — the "transmit" path */
static void tx_tasklet_fn(unsigned long data)
{
	ktime_t now = ktime_get();
	s64 since_start = ktime_to_us(ktime_sub(now, base));
	s64 measured_gap = ktime_to_us(ktime_sub(now, last_tx));
	s64 target = (s64)seq * gap_us;

	pr_info("pacing_lab: pkt %2d  t=+%6lld us  gap=%5lld us (target %d)  jitter %+lld us  [in_softirq=%d]\n",
		seq, since_start, measured_gap, gap_us,
		since_start - target, !!in_softirq());

	last_tx = now;
	seq++;

	if (seq <= packets)
		arm_next();
	else
		complete(&pacing_done);
}
static DECLARE_TASKLET(tx_tasklet, tx_tasklet_fn, 0);

/* top-half-ish: hardirq context — just kick the bottom half */
static enum hrtimer_restart pacing_timer_fn(struct hrtimer *t)
{
	tasklet_schedule(&tx_tasklet);	/* == raise a softirq */
	return HRTIMER_NORESTART;	/* the tasklet re-arms the next gap */
}

static int __init pacing_lab_init(void)
{
	if (gap_us < 1 || packets < 1)
		return -EINVAL;

	pr_info("pacing_lab: pacing %d packets at %d us gap (hrtimer -> tasklet)\n",
		packets, gap_us);

	hrtimer_init(&pacing_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	pacing_timer.function = pacing_timer_fn;

	base = ktime_get();
	last_tx = base;
	seq = 1;
	arm_next();

	wait_for_completion(&pacing_done);
	pr_info("pacing_lab: done, paced %d packets\n", packets);
	return 0;
}

static void __exit pacing_lab_exit(void)
{
	hrtimer_cancel(&pacing_timer);
	tasklet_kill(&tx_tasklet);
	pr_info("pacing_lab: unloaded\n");
}

module_init(pacing_lab_init);
module_exit(pacing_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("hrtimer-driven packet pacing (sch_fq style), no NIC");
