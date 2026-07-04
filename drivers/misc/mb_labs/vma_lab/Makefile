ifneq ($(KERNELRELEASE),)
# kbuild picks this branch when descending into the directory
obj-$(CONFIG_PROCESS_TASK) += kernel_addr.o
else
# standalone make builds the userspace probe
CC ?= gcc
CFLAGS ?= -g

all: user_addr

user_addr: user_addr.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

.PHONY: all clean
clean:
	rm -f user_addr *.o
endif
