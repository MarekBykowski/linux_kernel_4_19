// SPDX-License-Identifier: GPL-2.0
/*
 * radix tree lab: a sparse, tree-backed array of pointers. Entries
 * live at indices 10, 42 and 1000000000 without allocating a billion
 * slots - only the needed tree nodes exist.
 *
 * v4.20 replaced this API with the XArray (xa_store/xa_load), which
 * wraps the same underlying structure.
 */
#include <linux/module.h>
#include <linux/radix-tree.h>

static RADIX_TREE(my_tree, GFP_KERNEL);

static int values[] = { 100, 200, 300 };

static const unsigned long indices[] = { 42, 10, 1000000000 };

static int __init radix_tree_lab_init(void)
{
	struct radix_tree_iter iter;
	void __rcu **slot;
	int *ptr;
	int i;

	for (i = 0; i < ARRAY_SIZE(indices); i++) {
		int err = radix_tree_insert(&my_tree, indices[i], &values[i]);

		if (err)
			return err;
	}

	for (i = 0; i < ARRAY_SIZE(indices); i++) {
		ptr = radix_tree_lookup(&my_tree, indices[i]);
		pr_info("radix_tree_lab: lookup %lu -> %d\n",
			indices[i], ptr ? *ptr : -1);
	}

	rcu_read_lock();
	radix_tree_for_each_slot(slot, &my_tree, &iter, 0)
		pr_info("radix_tree_lab: iter: index=%lu entry=%px\n",
			iter.index, radix_tree_deref_slot(slot));
	rcu_read_unlock();

	return 0;
}

static void __exit radix_tree_lab_exit(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(indices); i++)
		radix_tree_delete(&my_tree, indices[i]);

	pr_info("radix_tree_lab: unloaded\n");
}

module_init(radix_tree_lab_init);
module_exit(radix_tree_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marek Bykowski");
MODULE_DESCRIPTION("Sparse pointer array via the radix tree");
