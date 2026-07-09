// SPDX-License-Identifier: GPL-2.0
/*
 * process/thread lab (kernel side): a misc char device /dev/mb_procthread that
 * userspace hammers from many pthreads AND many fork()'d processes at
 * once. Every write() bumps a shared counter under a mutex, so the
 * final value is exact no matter how much userspace concurrency hits
 * it — the kernel serialises the concurrent syscalls.
 *
 * Each open() logs the caller's identity as the kernel sees it:
 *   pid  = the kernel task id  (unique per *thread*)
 *   tgid = the thread-group id (shared by threads, == userspace getpid)
 * so pthreads of one process share a tgid with distinct pids, while
 * fork()'d processes each have their own tgid. That is the kernel-eye
 * view of "thread vs process".
 *
 * The word "pid" flips meaning between the layers — same concept, two
 * names. userspace PID == kernel tgid; userspace TID == kernel pid:
 *
 *   concept       userspace name   kernel field   syscall
 *   -----------   --------------   ------------   ---------
 *   process id    PID              tgid           getpid()
 *   thread id     TID              pid            gettid()
 *
 * So the kernel's `pid` is the thread and its `tgid` is the process;
 * userspace's getpid() returns the kernel tgid and gettid() the pid.
 *
 * Example dmesg from a run (3 pthreads + 2 fork()'d processes):
 *
 *   open by pid=415 tgid=415   <- main process
 *   open by pid=417 tgid=417   <- fork()'d process (own tgid)
 *   open by pid=418 tgid=418   <- fork()'d process (own tgid)
 *   open by pid=419 tgid=415   -.
 *   open by pid=420 tgid=415    |- 3 pthreads: SAME tgid (415), distinct pids
 *   open by pid=421 tgid=415   -'
 */
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/sched.h>

static DEFINE_MUTEX(lock);
static u64 counter;

static int pt_open(struct inode *inode, struct file *file)
{
	pr_info("procthread_lab: open by pid=%d tgid=%d comm=%s\n",
		current->pid, current->tgid, current->comm);
	return 0;
}

/* each write() call = one increment; the mutex makes it race-free */
static ssize_t pt_write(struct file *file, const char __user *buf,
			  size_t len, loff_t *ppos)
{
	mutex_lock(&lock);
	counter++;
	mutex_unlock(&lock);
	return len;
}

/* read() returns the current counter as a decimal string */
static ssize_t pt_read(struct file *file, char __user *buf,
			 size_t len, loff_t *ppos)
{
	char tmp[32];
	int n;

	if (*ppos)
		return 0;

	mutex_lock(&lock);
	n = scnprintf(tmp, sizeof(tmp), "%llu\n", counter);
	mutex_unlock(&lock);

	if (copy_to_user(buf, tmp, n))
		return -EFAULT;
	*ppos = n;
	return n;
}

static const struct file_operations pt_fops = {
	.owner = THIS_MODULE,
	.open = pt_open,
	.write = pt_write,
	.read = pt_read,
};

static struct miscdevice pt_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mb_procthread",
	.mode = 0666,
	.fops = &pt_fops,
};

static int __init procthread_lab_init(void)
{
	counter = 0;
	return misc_register(&pt_dev);
}

static void __exit procthread_lab_exit(void)
{
	pr_info("procthread_lab: final counter = %llu\n", counter);
	misc_deregister(&pt_dev);
}

module_init(procthread_lab_init);
module_exit(procthread_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("char device hammered by userspace threads and processes");
