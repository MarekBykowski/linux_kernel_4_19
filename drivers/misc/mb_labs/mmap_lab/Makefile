ifneq ($(KERNELRELEASE),)
# kbuild picks this branch when descending into the directory
obj-$(CONFIG_MB_LAB_MMAP) += mmap_lab.o
else
# standalone make builds the userspace half
CC ?= gcc
CFLAGS ?= -g
all: user_mmap
user_mmap: user_mmap.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@
.PHONY: all clean
clean:
	rm -f user_mmap *.o
endif
