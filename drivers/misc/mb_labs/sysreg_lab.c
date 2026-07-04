// SPDX-License-Identifier: GPL-2.0
/*
 * sysreg lab: read arm64 system registers with mrs (via read_sysreg)
 * and expose them decoded at /proc/mb_sysregs. Each cat runs on
 * whichever CPU the reader lands on — compare MPIDR affinity across
 * reads. Pairs with the SP_EL0 == current trick in the vma lab.
 */
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/smp.h>
#include <asm/sysreg.h>

static int sysreg_show(struct seq_file *m, void *v)
{
	u64 midr, mpidr, cntvct, cntfrq, cur_el;

	midr = read_sysreg(midr_el1);
	mpidr = read_sysreg(mpidr_el1);
	cntvct = read_sysreg(cntvct_el0);
	cntfrq = read_sysreg(cntfrq_el0);
	cur_el = read_sysreg(CurrentEL);

	seq_printf(m, "smp_processor_id : %d\n", smp_processor_id());
	seq_printf(m, "CurrentEL        : EL%llu\n", cur_el >> 2);
	seq_printf(m, "MIDR_EL1         : %016llx (implementer %#02llx%s, partnum %#03llx, r%llup%llu)\n",
		   midr, (midr >> 24) & 0xff,
		   ((midr >> 24) & 0xff) == 0x41 ? "=ARM" : "",
		   (midr >> 4) & 0xfff, (midr >> 20) & 0xf, midr & 0xf);
	seq_printf(m, "MPIDR_EL1        : %016llx (aff2 %llu, aff1 %llu, aff0 %llu)\n",
		   mpidr, (mpidr >> 16) & 0xff, (mpidr >> 8) & 0xff,
		   mpidr & 0xff);
	seq_printf(m, "CNTFRQ_EL0       : %llu Hz\n", cntfrq);
	seq_printf(m, "CNTVCT_EL0       : %llu (%llu s since counter start)\n",
		   cntvct, cntfrq ? cntvct / cntfrq : 0);

	return 0;
}

static int sysreg_open(struct inode *inode, struct file *file)
{
	return single_open(file, sysreg_show, NULL);
}

static const struct file_operations sysreg_fops = {
	.owner = THIS_MODULE,
	.open = sysreg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init sysreg_lab_init(void)
{
	if (!proc_create("mb_sysregs", 0444, NULL, &sysreg_fops))
		return -ENOMEM;

	pr_info("sysreg_lab: /proc/mb_sysregs created\n");
	return 0;
}

static void __exit sysreg_lab_exit(void)
{
	remove_proc_entry("mb_sysregs", NULL);
	pr_info("sysreg_lab: unloaded\n");
}

module_init(sysreg_lab_init);
module_exit(sysreg_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("arm64 system registers decoded in procfs");
