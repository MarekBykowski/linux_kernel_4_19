// SPDX-License-Identifier: GPL-2.0
/*
 * page-table walk lab: resolve a user virtual address to its physical
 * address by hand, level by level (pgd -> pud -> pmd -> pte), the way
 * the MMU does on a TLB miss. Walks the target's start_code and
 * start_stack. Modeled on arch/arm64/mm/fault.c:show_pte().
 *
 * Note 4.19 arm64 predates the p4d conversion: pud_offset() takes the
 * pgd pointer directly.
 */
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/sizes.h>

static int pid = 1;
module_param(pid, int, 0444);
MODULE_PARM_DESC(pid, "process whose address space to walk");

static void walk_one(struct mm_struct *mm, unsigned long addr,
		     const char *what)
{
	pgd_t *pgdp, pgd;
	pud_t *pudp, pud;
	pmd_t *pmdp, pmd;
	pte_t *ptep, pte;
	u64 phys;

	pr_info("ptwalk_lab: %s va=%016lx\n", what, addr);

	pgdp = pgd_offset(mm, addr);
	pgd = READ_ONCE(*pgdp);
	pr_info("  pgd @ %px = %016llx\n", pgdp, pgd_val(pgd));
	if (pgd_none(pgd) || pgd_bad(pgd))
		return;

	pudp = pud_offset(pgdp, addr);
	pud = READ_ONCE(*pudp);
	pr_info("  pud @ %px = %016llx\n", pudp, pud_val(pud));
	if (pud_none(pud) || pud_bad(pud))
		return;

	pmdp = pmd_offset(pudp, addr);
	pmd = READ_ONCE(*pmdp);
	pr_info("  pmd @ %px = %016llx\n", pmdp, pmd_val(pmd));
	/* a block (section) mapping fails pmd_bad; the walk ends there */
	if (pmd_none(pmd) || pmd_bad(pmd))
		return;

	ptep = pte_offset_map(pmdp, addr);
	pte = READ_ONCE(*ptep);
	pr_info("  pte @ %px = %016llx\n", ptep, pte_val(pte));

	if (pte_present(pte)) {
		phys = ((u64)pte_pfn(pte) << PAGE_SHIFT) | (addr & ~PAGE_MASK);
		pr_info("  pa=%016llx  attrs:%s%s%s%s%s%s\n", phys,
			pte_write(pte) ? " write" : " ro",
			pte_dirty(pte) ? " dirty" : "",
			pte_young(pte) ? " young(AF)" : "",
			(pte_val(pte) & PTE_UXN) ? " uxn" : " uexec",
			(pte_val(pte) & PTE_PXN) ? " pxn" : " pexec",
			(pte_val(pte) & PTE_NG) ? " ng" : "");
	} else {
		pr_info("  not present (swapped out or never faulted in)\n");
	}

	pte_unmap(ptep);
}

static int __init ptwalk_lab_init(void)
{
	struct task_struct *task;
	struct mm_struct *mm = NULL;

	for_each_process(task) {
		if (task->pid == pid) {
			mm = task->mm;
			break;
		}
	}

	if (!mm) {
		pr_info("ptwalk_lab: pid %d not found or has no mm\n", pid);
		return -ESRCH;
	}

	pr_info("ptwalk_lab: pid %d (%s), %luk pages, %u-bit VAs, pgd table @ %px\n",
		pid, task->comm, PAGE_SIZE / SZ_1K, VA_BITS, mm->pgd);

	down_read(&mm->mmap_sem);
	walk_one(mm, mm->start_code, "start_code");
	walk_one(mm, mm->start_stack, "start_stack");
	up_read(&mm->mmap_sem);

	return 0;
}

static void __exit ptwalk_lab_exit(void)
{
	pr_info("ptwalk_lab: unloaded\n");
}

module_init(ptwalk_lab_init);
module_exit(ptwalk_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("manual arm64 page-table walk of a user address");
