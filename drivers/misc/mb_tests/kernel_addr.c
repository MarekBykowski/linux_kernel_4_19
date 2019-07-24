#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/sched/signal.h>

static int pid_mem = 1;

__maybe_unused
static void print_mem(struct task_struct *task)
{
        struct mm_struct *mm;
        struct vm_area_struct *vma;
        int count = 0;
        mm = task->mm;

        printk("This mm_struct has %d vmas.\n", mm->map_count);
        for (vma = mm->mmap ; vma ; vma = vma->vm_next) {
                printk ("Vma number %d: \n", ++count);
                printk("  Starts at 0x%lx, Ends at 0x%lx\n",
                          vma->vm_start, vma->vm_end);
        }
        printk("Code  Segment start = 0x%lx, end = 0x%lx \n"
               "Data  Segment start = 0x%lx, end = 0x%lx\n"
               "Stack Segment start = 0x%lx\n",
                 mm->start_code, mm->end_code,
                 mm->start_data, mm->end_data,
                 mm->start_stack);
}

static int mm_exp_load(void){
        struct task_struct *task;

        for_each_process(task) {
			if ((task->pid == pid_mem) || (task->pid == current->pid)) {
					printk("task_struct/process descr name %s pid %d\n", task->comm, task->pid);
					/*print_mem(task);*/
			} 
        }

#if 1
{
		unsigned long spsel, sp_el0, sp_el1;
		asm volatile("mrs %0, spsel" : "=r" (spsel));
		asm volatile("mrs %0, sp_el0" : "=r" (sp_el0));
		asm volatile("mrs %0, sp" : "=r" (sp_el1));
		/*
		  reading sp_el1 throws undefined instruction. I can read it in EL2
		  though:
		  asm volatile("mrs %0, sp_el1" : "=r" (sp_el1));
		  sp_el1 = read_sysreg(sp_el1);*/

		pr_info("spsel %lx sp_el0 %lx sp_el1 %lx-cannot read\n",
				spsel, sp_el0, sp_el1);

		asm volatile("mrs %0, sp_el0" : "=r" (sp_el0));
		pr_info("sp_el0 %lx\n", sp_el0);
		pr_info("current read from sp_el0 %px\n", (void*) current);

		asm volatile("mrs %0, sp_el0" : "=r" (sp_el0));
		pr_info("sp_el0 %lx\n", sp_el0);
		pr_info("current read from sp_el0 %lx\n", (unsigned long) current);
}
#endif

		pr_info("\n");

#if 0
        for_each_process(task) {
			printk("task_struct/process descr name %s pid %d\n", task->comm, task->pid);
        }
#endif
        return 0;
}

static void mm_exp_unload(void)
{
        printk("\nPrint segment information module exiting.\n");
}

module_init(mm_exp_load);
module_exit(mm_exp_unload);
module_param(pid_mem, int, 0);

MODULE_AUTHOR ("Krishnakumar. R, rkrishnakumar@gmail.com");
MODULE_DESCRIPTION ("Print segment information");
MODULE_LICENSE("GPL");
