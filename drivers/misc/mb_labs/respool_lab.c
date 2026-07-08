// SPDX-License-Identifier: GPL-2.0
/*
 * resource-pool lab: the *applicable* use of a counting semaphore — a
 * bounded pool of N interchangeable hardware resources (think DMA
 * channels) shared by M > N client threads.
 *
 * This is the "rental" pattern: borrow a channel, use it, return it.
 * When all N are busy, callers BLOCK (backpressure) until one frees.
 * That "count N permits and block when none are left" is exactly what
 * a counting semaphore provides and a lock cannot:
 *
 *   - the semaphore answers "how many, and wait if full"  (admission)
 *   - a spinlock answers "which channel is free"          (bookkeeping)
 *
 * Real resource pools use BOTH. Contrast the window cache (lock + LRU,
 * never blocks): there exhaustion *reclaims*; here it *waits*. Different
 * policy -> different primitive.
 */
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/completion.h>

#define NCHAN    3		/* pool size == number of semaphore permits */
#define NCLIENTS 6		/* more clients than channels -> contention */
#define NJOBS    2		/* transfers per client */

static struct semaphore pool;		/* free-channel count; blocks at 0 */
static DEFINE_SPINLOCK(map_lock);	/* protects the busy[] bookkeeping */
static bool busy[NCHAN];

static atomic_t in_use = ATOMIC_INIT(0);
static atomic_t max_use = ATOMIC_INIT(0);
static atomic_t remaining;
static struct completion all_done;

/* the spinlock's job: pick a specific free channel (identity/bookkeeping) */
static int acquire_channel(void)
{
	int i, ch = -1;

	spin_lock(&map_lock);
	for (i = 0; i < NCHAN; i++) {
		if (!busy[i]) {
			busy[i] = true;
			ch = i;
			break;
		}
	}
	spin_unlock(&map_lock);
	return ch;	/* the semaphore guarantees one is free, so ch >= 0 */
}

static void release_channel(int ch)
{
	spin_lock(&map_lock);
	busy[ch] = false;
	spin_unlock(&map_lock);
}

static int client(void *arg)
{
	long id = (long)arg;
	int j;

	for (j = 0; j < NJOBS; j++) {
		int ch, now;

		pr_info("respool_lab: client %ld requesting a channel...\n", id);
		down(&pool);		/* BLOCKS here if all NCHAN are busy */

		ch = acquire_channel();
		now = atomic_inc_return(&in_use);
		if (now > atomic_read(&max_use))
			atomic_set(&max_use, now);
		pr_info("respool_lab: client %ld GOT channel %d (in use %d/%d)\n",
			id, ch, now, NCHAN);

		msleep(100 + 40 * (id % 3));	/* simulate a transfer */

		atomic_dec(&in_use);
		release_channel(ch);
		pr_info("respool_lab: client %ld released channel %d\n", id, ch);
		up(&pool);		/* return the permit, wakes a waiter */
	}

	if (atomic_dec_and_test(&remaining))
		complete(&all_done);
	return 0;
}

static int __init respool_lab_init(void)
{
	long i;

	sema_init(&pool, NCHAN);		/* N permits == N channels */
	atomic_set(&remaining, NCLIENTS);
	init_completion(&all_done);

	pr_info("respool_lab: %d channels, %d clients -> max %d concurrent, rest block\n",
		NCHAN, NCLIENTS, NCHAN);

	for (i = 0; i < NCLIENTS; i++)
		kthread_run(client, (void *)i, "respool_%ld", i);

	wait_for_completion(&all_done);
	pr_info("respool_lab: done — peak concurrent channels = %d (of %d), never exceeded\n",
		atomic_read(&max_use), NCHAN);
	return 0;
}

static void __exit respool_lab_exit(void)
{
	pr_info("respool_lab: unloaded\n");
}

module_init(respool_lab_init);
module_exit(respool_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("counting semaphore: bounded resource pool with blocking backpressure");
