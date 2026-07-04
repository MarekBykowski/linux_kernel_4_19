#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int our_init_data = 30;
int our_noinit_data;

void our_prints(void)
{
        int our_local_data = 1;
        printf("\nPid of the process is = %d", getpid());
        printf("\nAddresses which fall into:");
        printf("\n 1) .data  sec = %p",
                &our_init_data);
        printf("\n 2) .BSS   sec = %p",
                &our_noinit_data);
	printf("\n 3) sbrk\t = %p",
		(void*)sbrk(0));
        printf("\n 4) .text  sec = %p",
                &our_prints);
        printf("\n 5) .stack sec = %p\n",
                &our_local_data);
		
	while(1);

}

int main(int argc, char *argv[])
{
        our_prints();
        return 0;
}
