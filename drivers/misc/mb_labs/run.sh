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

run_lab procfs_lab poke_procfs
run_lab completion_lab
run_lab waitqueue_lab
run_lab workqueue_lab
run_lab radix_tree_lab
run_lab rcu_lab sleep 2

echo "=== vma_lab ==="
vma-lab-run.sh
