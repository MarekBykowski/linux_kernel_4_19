/*
 * Userspace half of the mmap lab: map the kernel page exported by
 * /dev/mmap_lab, read the kernel's message from it, overwrite it in
 * place, then read() it back through the device to prove both sides
 * touch the same physical page.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

int main(void)
{
	char buf[128];
	ssize_t n;
	char *p;
	int fd;

	fd = open("/dev/mmap_lab", O_RDWR);
	if (fd < 0) {
		perror("open /dev/mmap_lab");
		return 1;
	}

	p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		perror("mmap");
		return 1;
	}

	printf("mapped the kernel page at user va %p\n", p);
	printf("kernel wrote: \"%s\"\n", p);

	strcpy(p, "hello from userspace via the shared mapping");
	printf("overwrote the page through the mapping\n");

	n = pread(fd, buf, sizeof(buf) - 1, 0);
	if (n < 0) {
		perror("pread");
		return 1;
	}
	buf[n] = '\0';
	printf("read() from the device now returns: \"%s\"\n", buf);

	munmap(p, 4096);
	close(fd);
	return 0;
}
