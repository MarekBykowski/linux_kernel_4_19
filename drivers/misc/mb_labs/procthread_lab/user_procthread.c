/*
 * process/thread lab (userspace side): drive /dev/mb_procthread from both
 * multithreading (pthreads) and multiprocessing (fork), concurrently,
 * then verify the kernel counter is exact.
 *
 *   - NTHREADS pthreads share this process's address space and tgid
 *   - NPROCS fork()'d children each get their own address space + tgid
 *
 * Each worker opens the device (so the kernel logs its pid/tgid),
 * does NINC write()s (each = one kernel-side increment), and closes.
 * Total expected increments = (NTHREADS + NPROCS) * NINC.
 *
 * Watch dmesg: pthreads log the same tgid with different pids; the
 * fork()'d processes log distinct tgids — the kernel's view of
 * "thread vs process".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/syscall.h>

#define DEV       "/dev/mb_procthread"
#define NTHREADS  3
#define NPROCS    2
#define NINC      20000

static long read_counter(void)
{
	char buf[32];
	int fd = open(DEV, O_RDONLY);
	long v = -1;

	if (fd >= 0) {
		ssize_t n = read(fd, buf, sizeof(buf) - 1);

		if (n > 0) {
			buf[n] = '\0';
			v = atol(buf);
		}
		close(fd);
	}
	return v;
}

/* one worker: open, increment NINC times, close */
static void hammer(const char *who)
{
	int fd = open(DEV, O_RDWR);
	int i;

	if (fd < 0) {
		perror("open " DEV);
		return;
	}
	printf("%s: pid=%d tid=%ld hammering %d times\n",
	       who, getpid(), syscall(SYS_gettid), NINC);
	fflush(stdout);		/* fork'd children _exit() without flushing stdio */
	for (i = 0; i < NINC; i++)
		if (write(fd, "x", 1) != 1)
			break;
	close(fd);
}

static void *thread_fn(void *arg)
{
	hammer("thread");
	return NULL;
}

int main(void)
{
	pthread_t t[NTHREADS];
	long start, end, expected;
	int i;

	start = read_counter();
	printf("counter before: %ld\n", start);
	fflush(stdout);		/* don't leave buffered output to duplicate across fork */

	/* multiprocessing: fork NPROCS children that hammer */
	for (i = 0; i < NPROCS; i++) {
		pid_t pid = fork();

		if (pid == 0) {
			hammer("process");
			_exit(0);
		}
	}

	/* multithreading: NTHREADS pthreads that hammer */
	for (i = 0; i < NTHREADS; i++)
		pthread_create(&t[i], NULL, thread_fn, NULL);

	for (i = 0; i < NTHREADS; i++)
		pthread_join(t[i], NULL);
	for (i = 0; i < NPROCS; i++)
		wait(NULL);

	end = read_counter();
	expected = (long)(NTHREADS + NPROCS) * NINC;
	printf("counter after:  %ld  (delta %ld, expected %ld) -> %s\n",
	       end, end - start, expected,
	       (end - start == expected) ? "PASS" : "FAIL");
	return 0;
}
