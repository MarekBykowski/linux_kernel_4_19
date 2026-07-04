// SPDX-License-Identifier: GPL-2.0
/*
 * seqlock lab: lockless readers that retry instead of blocking. The
 * writer bumps a sequence counter around its update; a reader that
 * overlaps a write sees an odd or changed sequence in read_seqretry()
 * and simply loops. The two fields are always read consistent —
 * a==b every time — even though the reader takes no lock.
 */
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/seqlock.h>
#include <linux/delay.h>
#include <linux/completion.h>

#define READS 1000

static seqlock_t my_seqlock;
static u64 val_a, val_b;

static struct task_struct *writer;
static DECLARE_COMPLETION(reader_done);

static int writer_thread(void *unused)
{
	while (!kthread_should_stop()) {
		write_seqlock(&my_seqlock);
		val_a++;
		udelay(100);	/* widen the window a reader can land in */
		val_b++;
		write_sequnlock(&my_seqlock);
		usleep_range(500, 1000);
	}
	return 0;
}

static int reader_thread(void *unused)
{
	unsigned long retries = 0;
	u64 a, b;
	unsigned int seq;
	int i;

	for (i = 0; i < READS; i++) {
		do {
			seq = read_seqbegin(&my_seqlock);
			a = val_a;
			b = val_b;
			if (read_seqretry(&my_seqlock, seq))
				retries++;
			else
				break;
		} while (1);

		if (a != b)
			pr_err("seqlock_lab: TORN READ a=%llu b=%llu\n", a, b);
		usleep_range(100, 500);
	}

	pr_info("seqlock_lab: %d reads, %lu retries, no torn reads, final a=b=%llu\n",
		READS, retries, a);
	complete(&reader_done);
	return 0;
}

static int __init seqlock_lab_init(void)
{
	seqlock_init(&my_seqlock);
	pr_info("seqlock_lab: writer updating two fields, reader checking consistency\n");

	writer = kthread_run(writer_thread, NULL, "mb_seq_writer");
	kthread_run(reader_thread, NULL, "mb_seq_reader");

	wait_for_completion(&reader_done);
	return 0;
}

static void __exit seqlock_lab_exit(void)
{
	kthread_stop(writer);
	pr_info("seqlock_lab: unloaded\n");
}

module_init(seqlock_lab_init);
module_exit(seqlock_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("consistent lockless reads with a seqlock");
