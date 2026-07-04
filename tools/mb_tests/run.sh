#!/bin/bash

rmmod kernel_addr
dmesg -C
insmod ./kernel_addr.ko
dmesg
