ifneq ($(KERNELRELEASE),)
# kbuild picks this branch when descending into the directory
obj-$(CONFIG_MB_LAB_PROCTHREAD) += procthread_lab.o
else
# standalone make builds the userspace multithread/multiprocess driver
CC ?= gcc
CFLAGS ?= -g -O2
all: user_procthread
user_procthread: user_procthread.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ -lpthread
.PHONY: all clean
clean:
	rm -f user_procthread *.o
endif
