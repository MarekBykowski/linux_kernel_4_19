// SPDX-License-Identifier: GPL-2.0
/*
 * threaded IRQ lab: the two-halves interrupt model without hardware.
 *
 * request_threaded_irq() splits an interrupt into
 *   - a primary (top half) handler that runs in interrupt context, does
 *     the minimal ack, and returns IRQ_WAKE_THREAD, and
 *   - a thread_fn (bottom half) that runs in a dedicated kernel thread
 *     ("irq/<n>-irqthread_lab"), in process context, and so MAY SLEEP.
 *
 * The FVP has no spare device interrupt, so the source is synthetic:
 * the kernel's IRQ simulator (CONFIG_IRQ_SIM) hands out a real Linux
 * irq number that can be requested like any other and fired from
 * software with irq_sim_fire(). The threading is genuine — only the
 * stimulus is fake.
 */
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq_sim.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/sched.h>

static struct irq_sim sim;
static int lab_irq = -1;
static DECLARE_COMPLETION(thread_done);

static void report_context(const char *where)
{
	pr_info("irqthread_lab: %-7s in_task=%d in_softirq=%d in_irq=%d in_atomic=%d | comm=%-18s pid=%d\n",
		where, !!in_task(), !!in_softirq(), !!in_irq(), !!in_atomic(),
		current->comm, current->pid);
}

/* top half: interrupt context, must be quick, wakes the thread */
static irqreturn_t lab_primary(int irq, void *dev)
{
	report_context("primary");
	return IRQ_WAKE_THREAD;
}

/* bottom half: its own kthread, process context, may sleep */
static irqreturn_t lab_thread(int irq, void *dev)
{
	report_context("thread");
	msleep(10);	/* legal here — proves the bottom half can sleep */
	pr_info("irqthread_lab: thread  slept 10ms and handled irq %d\n", irq);
	complete(&thread_done);
	return IRQ_HANDLED;
}

static int __init irqthread_lab_init(void)
{
	int ret;

	ret = irq_sim_init(&sim, 1);
	if (ret)
		return ret;

	lab_irq = irq_sim_irqnum(&sim, 0);
	pr_info("irqthread_lab: simulated irq = %d\n", lab_irq);

	ret = request_threaded_irq(lab_irq, lab_primary, lab_thread,
				   0, "irqthread_lab", &sim);
	if (ret) {
		irq_sim_fini(&sim);
		return ret;
	}

	report_context("init");
	pr_info("irqthread_lab: firing the irq from software\n");
	irq_sim_fire(&sim, 0);

	wait_for_completion(&thread_done);
	return 0;
}

static void __exit irqthread_lab_exit(void)
{
	free_irq(lab_irq, &sim);	/* also stops the irq thread */
	irq_sim_fini(&sim);
	pr_info("irqthread_lab: unloaded\n");
}

module_init(irqthread_lab_init);
module_exit(irqthread_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("threaded IRQ via the software irq simulator");
