// SPDX-License-Identifier: GPL-2.0
/*
 * IPI lab: interrupt a chosen CPU and run a callback on it.
 * smp_call_function_single() sends a GIC SGI (software generated
 * interrupt) to the target core; the callback runs there in hard
 * interrupt context, on top of whatever that CPU was doing. Watch the
 * "Function call interrupts" row of /proc/interrupts tick up.
 *
 * Load with cpu=N to pick the target. If the sender and target are
 * the same CPU no IPI is needed — the callback is invoked directly.
 */
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/hardirq.h>
#include <asm/sysreg.h>

static int cpu = 1;
module_param(cpu, int, 0444);
MODULE_PARM_DESC(cpu, "CPU to interrupt and run the callback on");

static void ipi_callback(void *info)
{
	u64 mpidr = read_sysreg(mpidr_el1);

	pr_info("ipi_lab: callback on CPU %d (MPIDR aff %llx), in_irq=%lu, interrupted \"%s\" (pid %d)\n",
		smp_processor_id(), mpidr & 0xffffff, in_irq(),
		current->comm, current->pid);
}

static int __init ipi_lab_init(void)
{
	int sender, ret;

	get_online_cpus();

	if (!cpu_online(cpu)) {
		pr_info("ipi_lab: cpu %d is not online\n", cpu);
		put_online_cpus();
		return -EINVAL;
	}

	sender = get_cpu();
	pr_info("ipi_lab: CPU %d sending IPI to CPU %d\n", sender, cpu);

	/* wait=1: spin until the target CPU has run the callback */
	ret = smp_call_function_single(cpu, ipi_callback, NULL, 1);
	put_cpu();
	put_online_cpus();

	if (ret) {
		pr_info("ipi_lab: smp_call_function_single failed: %d\n", ret);
		return ret;
	}

	pr_info("ipi_lab: callback completed on CPU %d\n", cpu);
	return 0;
}

static void __exit ipi_lab_exit(void)
{
	pr_info("ipi_lab: unloaded\n");
}

module_init(ipi_lab_init);
module_exit(ipi_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("interrupt a chosen CPU and run a callback there");
