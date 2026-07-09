#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/sched/signal.h>

static int pid_mem = 1;

static void print_mem(struct task_struct *task)
{
	struct mm_struct *mm = task->mm;
	struct vm_area_struct *vma;
	int n = 0;

	if (!mm) {
		pr_info("  %s (pid %d) is a kernel thread — no mm\n",
			task->comm, task->pid);
		return;
	}

	down_read(&mm->mmap_sem);

	pr_info("process %s (pid %d, tgid %d) — %d VMAs\n",
		task->comm, task->pid, task->tgid, mm->map_count);
	pr_info("  %-4s %-25s %7s  %-4s %s\n",
		"#", "range", "size", "perm", "backing");

	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		unsigned long kb = (vma->vm_end - vma->vm_start) >> 10;
		const char *backing;
		char perm[5];

		perm[0] = (vma->vm_flags & VM_READ)   ? 'r' : '-';
		perm[1] = (vma->vm_flags & VM_WRITE)  ? 'w' : '-';
		perm[2] = (vma->vm_flags & VM_EXEC)   ? 'x' : '-';
		perm[3] = (vma->vm_flags & VM_SHARED) ? 's' : 'p';
		perm[4] = '\0';

		if (vma->vm_file)		/* file-backed: show the basename */
			backing = vma->vm_file->f_path.dentry->d_name.name;
		else if (vma->vm_start <= mm->brk &&
			 vma->vm_end >= mm->start_brk)
			backing = "[heap]";
		else if (vma->vm_start <= mm->start_stack &&
			 vma->vm_end >= mm->start_stack)
			backing = "[stack]";
		else
			backing = "[anon]";

		pr_info("  %-4d 0x%010lx-0x%010lx %6luK  %-4s %s\n",
			++n, vma->vm_start, vma->vm_end, kb, perm, backing);
	}

	pr_info("  segments: code 0x%lx-0x%lx  data 0x%lx-0x%lx  stack 0x%lx\n",
		mm->start_code, mm->end_code,
		mm->start_data, mm->end_data, mm->start_stack);

	up_read(&mm->mmap_sem);
}

static int mm_exp_load(void)
{
	struct task_struct *task;

	/* the target process (pid_mem) and ourselves (the modprobe task) */
	for_each_process(task)
		if (task->pid == pid_mem || task->pid == current->pid) {
			print_mem(task);
			pr_info("\n");
		}

	/*
	 * arm64: `current` is read from SP_EL0. The kernel runs with SPSel=1
	 * (its stack is SP_EL1/EL2), leaving SP_EL0 free to hold the current
	 * task pointer — so SP_EL0 == current. (SP_EL1 can't be read with mrs
	 * from EL1; `mov x, sp` gives the in-use stack instead.)
	 */
	{
		unsigned long spsel, sp_el0, sp_cur;

		asm volatile("mrs %0, spsel"  : "=r" (spsel));
		asm volatile("mrs %0, sp_el0" : "=r" (sp_el0));
		asm volatile("mov %0, sp"     : "=r" (sp_cur));

		pr_info("arm64: SPSel=%lu -> kernel stack is SP_EL%lu (SP=0x%lx)\n",
			spsel, spsel, sp_cur);
		pr_info("arm64: SP_EL0=0x%lx, current=0x%px -> current %s SP_EL0\n",
			sp_el0, current,
			sp_el0 == (unsigned long)current ? "==" : "!=");
	}

	return 0;
}

static void mm_exp_unload(void)
{
	pr_info("kernel_addr: module exiting\n");
}

module_init(mm_exp_load);
module_exit(mm_exp_unload);
module_param(pid_mem, int, 0);

MODULE_AUTHOR("Krishnakumar. R, rkrishnakumar@gmail.com");
MODULE_DESCRIPTION("Print segment information");
MODULE_LICENSE("GPL");
