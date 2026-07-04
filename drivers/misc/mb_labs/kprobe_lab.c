// SPDX-License-Identifier: GPL-2.0
/*
 * kprobes lab: plant a probe on a live kernel function without
 * rebuilding or rebooting. The pre-handler runs just before every
 * do_sys_open() call, on a breakpoint the kprobes core patches into
 * the function's first instruction (BRK on arm64).
 */
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/sched.h>

#define LOG_FIRST 5

static unsigned long hits;

static int open_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	hits++;
	if (hits <= LOG_FIRST)
		pr_info("kprobe_lab: do_sys_open hit #%lu from %s (pid %d)\n",
			hits, current->comm, current->pid);
	return 0;
}

static struct kprobe kp = {
	.symbol_name = "do_sys_open",
	.pre_handler = open_pre_handler,
};

static int __init kprobe_lab_init(void)
{
	int ret;

	ret = register_kprobe(&kp);
	if (ret) {
		pr_info("kprobe_lab: register_kprobe failed: %d\n", ret);
		return ret;
	}

	pr_info("kprobe_lab: probe planted at %px (%s), logging first %d hits\n",
		kp.addr, kp.symbol_name, LOG_FIRST);
	return 0;
}

static void __exit kprobe_lab_exit(void)
{
	unregister_kprobe(&kp);
	pr_info("kprobe_lab: unloaded, do_sys_open was hit %lu times\n", hits);
}

module_init(kprobe_lab_init);
module_exit(kprobe_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("kprobe on do_sys_open counting file opens");
