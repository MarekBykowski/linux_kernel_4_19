// SPDX-License-Identifier: GPL-2.0
/*
 * procfs lab: a boolean knob at /proc/my_bool
 *
 *   echo 1 > /proc/my_bool
 *   cat /proc/my_bool
 */
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#define PROC_NAME "my_bool"

static bool my_bool;

static ssize_t my_bool_read(struct file *file, char __user *buf,
			    size_t count, loff_t *ppos)
{
	char tmp[4];
	int len;

	/* one-shot read */
	if (*ppos != 0)
		return 0;

	len = scnprintf(tmp, sizeof(tmp), "%d\n", my_bool);

	if (copy_to_user(buf, tmp, len))
		return -EFAULT;

	*ppos = len;
	return len;
}

static ssize_t my_bool_write(struct file *file, const char __user *buf,
			     size_t count, loff_t *ppos)
{
	int val;

	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	my_bool = !!val;
	return count;
}

static const struct file_operations my_bool_ops = {
	.read   = my_bool_read,
	.write  = my_bool_write,
	.llseek = noop_llseek,
};

static int __init procfs_lab_init(void)
{
	if (!proc_create(PROC_NAME, 0666, NULL, &my_bool_ops))
		return -ENOMEM;

	pr_info("procfs_lab: /proc/%s created\n", PROC_NAME);
	return 0;
}

static void __exit procfs_lab_exit(void)
{
	remove_proc_entry(PROC_NAME, NULL);
	pr_info("procfs_lab: /proc/%s removed\n", PROC_NAME);
}

module_init(procfs_lab_init);
module_exit(procfs_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("Simplest procfs boolean driver");
