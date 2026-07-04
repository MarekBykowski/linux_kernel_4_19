#!/bin/sh
# Run the vma lab exercise: start the userspace probe, then inspect
# the same process from the kernel via the kernel_addr module.

rmmod kernel_addr 2>/dev/null
# busybox dmesg has no -C; -c prints and clears, so discard the print
dmesg -c >/dev/null

user_addr &
PID=$!
sleep 1

modprobe kernel_addr pid_mem=$PID
dmesg

kill $PID
rmmod kernel_addr
