// SPDX-License-Identifier: GPL-2.0
/*
 * list lab: the kernel's intrusive doubly-linked list (struct list_head).
 *
 * The list is *intrusive*: the link nodes live inside the objects, not in
 * separate allocations. Given a list_head pointer, container_of() recovers
 * the enclosing object by subtracting the member offset — that is the whole
 * trick, and why one object can sit on several lists at once.
 *
 * The list is circular and doubly-linked with a sentinel head, so the ends
 * need no NULL special-casing and list_add/list_del are branchless.
 */
#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>

struct item {
	int id;
	struct list_head node;	/* the embedded link */
};

static LIST_HEAD(my_list);	/* sentinel head; empty = points to itself */

static int __init list_lab_init(void)
{
	struct item *it, *tmp;
	int i;

	pr_info("list_lab: list_empty at start: %d\n", list_empty(&my_list));

	/* build: append three objects with an embedded node each */
	for (i = 1; i <= 3; i++) {
		it = kmalloc(sizeof(*it), GFP_KERNEL);
		if (!it)
			goto drain;
		it->id = i * 10;
		list_add_tail(&it->node, &my_list);	/* tail = FIFO order */
		pr_info("list_lab: added item id=%d\n", it->id);
	}

	/* iterate: list_for_each_entry hides the container_of */
	list_for_each_entry(it, &my_list, node)
		pr_info("list_lab: walk id=%d\n", it->id);

	/* one explicit container_of to show what the macro does */
	it = list_first_entry(&my_list, struct item, node);
	pr_info("list_lab: first node@%px -> item@%px id=%d\n",
		&it->node, it, it->id);

drain:
	/* teardown: _safe caches the next pointer so we can free while iterating */
	list_for_each_entry_safe(it, tmp, &my_list, node) {
		pr_info("list_lab: del+free id=%d\n", it->id);
		list_del(&it->node);
		kfree(it);
	}

	pr_info("list_lab: list_empty after drain: %d\n", list_empty(&my_list));
	return 0;
}

static void __exit list_lab_exit(void)
{
	pr_info("list_lab: unloaded\n");
}

module_init(list_lab_init);
module_exit(list_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("kernel intrusive doubly-linked list (list_head) example");
