/*
 * user_addr: a process printing its OWN view of its address space —
 * a representative address in each segment (.text/.data/.bss/heap/stack).
 * Compare these against kernel_addr's VMA dump of the same pid: each
 * address here falls inside one of the VMAs there.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int our_init_data = 30;		/* .data  (initialised global)   */
int our_noinit_data;		/* .bss   (uninitialised global) */

static void our_prints(void)
{
	int our_local_data = 1;	/* .stack (local variable)       */

	printf("user_addr: pid %d — my own view of my address space\n",
	       getpid());
	printf("  %-24s %p\n", ".text  (code)",          (void *)&our_prints);
	printf("  %-24s %p\n", ".data  (init global)",   (void *)&our_init_data);
	printf("  %-24s %p\n", ".bss   (uninit global)", (void *)&our_noinit_data);
	printf("  %-24s %p\n", "heap   (sbrk(0))",       (void *)sbrk(0));
	printf("  %-24s %p\n", ".stack (local var)",     (void *)&our_local_data);
	printf("  (staying alive for kernel_addr to inspect me — kill to exit)\n");

	fflush(stdout);		/* flush before we block, so output shows over pipes */

	pause();		/* block until killed — no busy-spin */
}

int main(void)
{
	our_prints();
	return 0;
}
