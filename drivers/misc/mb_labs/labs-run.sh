#!/bin/sh
# Run every mb_labs exercise in turn and print each lab's kernel log.

# busybox dmesg has no -C; -c prints and clears, so discard the print
run_lab() {
	lab=$1
	shift
	echo "=== $lab ==="
	dmesg -c >/dev/null
	modprobe "$lab" || { echo "$lab: modprobe failed"; return 1; }
	[ $# -gt 0 ] && "$@"
	rmmod "$lab"
	dmesg
	echo
}

poke_procfs() {
	echo "read: $(cat /proc/my_bool)"
	echo 1 > /proc/my_bool
	echo "after write 1: $(cat /proc/my_bool)"
}

poke_sysregs() {
	cat /proc/mb_sysregs
}

trigger_opens() {
	cat /etc/hostname >/dev/null
	cat /proc/uptime >/dev/null
}

run_lab procfs_lab poke_procfs
run_lab completion_lab
run_lab waitqueue_lab
run_lab workqueue_lab
run_lab radix_tree_lab
run_lab rcu_lab sleep 2
run_lab locking_lab
run_lab lockdep_lab
run_lab atomics_lab

echo "=== ptwalk_lab ==="
dmesg -c >/dev/null
user_addr >/dev/null &
PID=$!
sleep 1
modprobe ptwalk_lab pid=$PID
rmmod ptwalk_lab
kill $PID
dmesg
echo

run_lab kprobe_lab trigger_opens
run_lab mmap_lab user_mmap
run_lab seqlock_lab
run_lab sysreg_lab poke_sysregs
run_lab hrtimer_lab

echo "=== vma_lab ==="
vma-lab-run.sh
