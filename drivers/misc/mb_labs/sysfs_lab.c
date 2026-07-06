// SPDX-License-Identifier: GPL-2.0
/*
 * sysfs lab: expose a tunable to userspace the modern way — one value
 * per file under /sys, with show/store callbacks. This is the
 * preferred interface for driver/device attributes (procfs is legacy).
 *
 * The SAME int (my_value) is exposed two ways to contrast them:
 *   /sys/module/sysfs_lab/parameters/my_value  -- via module_param (free)
 *   /sys/kernel/mb_sysfs/value                  -- via a hand-written
 *                                                  kobject attribute
 * Also settable at load: insmod sysfs_lab.ko my_value=100
 *
 * /sys/kernel/mb_sysfs/ also has:
 *   value  (rw, 0644)  read/write the int via cat / echo
 *   info   (ro, 0444)  a read-only status string
 */
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

static int my_value = 42;
module_param(my_value, int, 0644);	/* also at /sys/module/.../parameters/ */
MODULE_PARM_DESC(my_value, "the tunable, also exposed as a kobject attribute");

static struct kobject *mb_kobj;

/* rw attribute: show is called on read (cat), store on write (echo) */
static ssize_t value_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	return sprintf(buf, "%d\n", my_value);	/* buf is one PAGE_SIZE page */
}

static ssize_t value_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t count)
{
	int ret = kstrtoint(buf, 10, &my_value);

	if (ret < 0)
		return ret;
	pr_info("sysfs_lab: value set to %d\n", my_value);
	return count;			/* return bytes consumed */
}
static struct kobj_attribute value_attr =
	__ATTR(value, 0644, value_show, value_store);

/* ro attribute: __ATTR_RO wires name "info" to info_show at mode 0444 */
static ssize_t info_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buf)
{
	return sprintf(buf, "mb sysfs lab: value=%d (read-only view)\n",
		       my_value);
}
static struct kobj_attribute info_attr = __ATTR_RO(info);

static struct attribute *mb_attrs[] = {
	&value_attr.attr,
	&info_attr.attr,
	NULL,				/* NULL-terminated */
};
static const struct attribute_group mb_attr_group = {
	.attrs = mb_attrs,
};

static int __init sysfs_lab_init(void)
{
	int ret;

	/* create the directory /sys/kernel/mb_sysfs */
	mb_kobj = kobject_create_and_add("mb_sysfs", kernel_kobj);
	if (!mb_kobj)
		return -ENOMEM;

	/* populate it with the attribute files */
	ret = sysfs_create_group(mb_kobj, &mb_attr_group);
	if (ret) {
		kobject_put(mb_kobj);
		return ret;
	}

	pr_info("sysfs_lab: /sys/kernel/mb_sysfs/{value,info} created\n");
	return 0;
}

static void __exit sysfs_lab_exit(void)
{
	kobject_put(mb_kobj);		/* removes the dir and its attributes */
	pr_info("sysfs_lab: unloaded\n");
}

module_init(sysfs_lab_init);
module_exit(sysfs_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("expose a tunable via sysfs (show/store)");
