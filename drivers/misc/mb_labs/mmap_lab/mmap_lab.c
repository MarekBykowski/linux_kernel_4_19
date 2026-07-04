// SPDX-License-Identifier: GPL-2.0
/*
 * mmap lab: hand a kernel page to userspace with zero copies. The
 * misc device's .mmap inserts the page's PFN straight into the
 * caller's page tables with remap_pfn_range(), so kernel and user
 * space then read and write the same physical page. The .read fop
 * returns the buffer through the normal copy path for comparison.
 */
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/gfp.h>

static char *shared_page;

static int mmap_lab_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long size = vma->vm_end - vma->vm_start;

	if (size > PAGE_SIZE)
		return -EINVAL;

	pr_info("mmap_lab: mapping pfn %lx (pa %llx) at user va %lx\n",
		virt_to_phys(shared_page) >> PAGE_SHIFT,
		(u64)virt_to_phys(shared_page), vma->vm_start);

	return remap_pfn_range(vma, vma->vm_start,
			       virt_to_phys(shared_page) >> PAGE_SHIFT,
			       size, vma->vm_page_prot);
}

static ssize_t mmap_lab_read(struct file *file, char __user *buf,
			     size_t count, loff_t *ppos)
{
	return simple_read_from_buffer(buf, count, ppos, shared_page,
				       strnlen(shared_page, PAGE_SIZE));
}

static const struct file_operations mmap_lab_fops = {
	.owner = THIS_MODULE,
	.mmap = mmap_lab_mmap,
	.read = mmap_lab_read,
};

static struct miscdevice mmap_lab_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mmap_lab",
	.mode = 0666,
	.fops = &mmap_lab_fops,
};

static int __init mmap_lab_init(void)
{
	int ret;

	shared_page = (char *)__get_free_page(GFP_KERNEL | __GFP_ZERO);
	if (!shared_page)
		return -ENOMEM;

	/* keep the VM from treating the remapped page as ordinary memory */
	SetPageReserved(virt_to_page(shared_page));

	strcpy(shared_page, "hello from the kernel, straight off a shared page");

	ret = misc_register(&mmap_lab_dev);
	if (ret) {
		ClearPageReserved(virt_to_page(shared_page));
		free_page((unsigned long)shared_page);
		return ret;
	}

	pr_info("mmap_lab: /dev/mmap_lab ready, kernel va %px\n", shared_page);
	return 0;
}

static void __exit mmap_lab_exit(void)
{
	pr_info("mmap_lab: page now holds: \"%s\"\n", shared_page);
	misc_deregister(&mmap_lab_dev);
	ClearPageReserved(virt_to_page(shared_page));
	free_page((unsigned long)shared_page);
	pr_info("mmap_lab: unloaded\n");
}

module_init(mmap_lab_init);
module_exit(mmap_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("share a kernel page with userspace via remap_pfn_range");
