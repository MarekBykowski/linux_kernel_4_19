// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 Intel
 /*
  * drivers/edac/axxia_edac-l2_cpu.c
  *
  * EDAC Driver for Intel's Axxia 5500 A15 CPUs and L2 caches
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program.
  *
  */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/axxia-ncr.h>
#include <linux/edac.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include "edac_module.h"
#include "axxia_edac.h"
#include "axxia_l2_55xx.h"

#define AXXIA_EDAC_MOD_STR     "axxia_edac"
#define CORES_PER_CLUSTER 4

#define APB2_PERSIST_SCRATCH 0xdc
#define L2_PERSIST_SCRATCH_BIT (0x1 << 5)
#define CPU_PERSIST_SCRATCH_BIT (0x1 << 6)

/* Private structure for common edac device */
struct axxia_edac_dev_info {
	struct platform_device *pdev;
	char *ctl_name;
	char *blk_name;
	int edac_idx;
	struct regmap *syscon;
	struct edac_device_ctl_info *edac_dev;
	void (*check)(struct edac_device_ctl_info *edac_dev);
};

void log_cpumerrsr(void *edac)
{
	struct edac_device_ctl_info *edac_dev = edac;
	u64 val, clear_val;
	u32 count0, count1;
	int i;
	struct axxia_edac_dev_info *dev_info;

	dev_info = edac_dev->pvt_info;

	/* Read cp15 for CPUMERRSR counts */
	val = read_cpumerrsr();
	if (val & 0x80000000) {
		int cpu = get_cpu();

		count0 = ((val) & 0x000000ff00000000) >> 20;
		count1 = ((val) & 0x0000ff0000000000) >> 28;

		/* increment correctable error counts */
		for (i = 0; i < count0 + count1; i++) {
			edac_device_handle_ce(edac_dev, 0,
					      cpu, edac_dev->ctl_name);
		}

		/* Clear the valid bit */
		clear_val = 0x80000000;
		write_cpumerrsr(clear_val);
		put_cpu();
	}
	if (val & 0x8000000000000000) {
		regmap_update_bits(dev_info->syscon,
				   APB2_PERSIST_SCRATCH,
				   CPU_PERSIST_SCRATCH_BIT,
				   CPU_PERSIST_SCRATCH_BIT);
		pr_emerg("CPU uncorrectable error\n");
		machine_restart(NULL);
	}
}

/* Check for CPU Errors */
static void axxia_cpu_error_check(struct edac_device_ctl_info *edac_dev)
{
	/* execute on current cpu */
	log_cpumerrsr(edac_dev);

	/* send ipis to execute on other cpus */
	smp_call_function(log_cpumerrsr, edac_dev, 1);
}

void log_l2merrsr(void *edac)
{
	struct edac_device_ctl_info *edac_dev = edac;
	u64 val, clear_val;
	u32 count0, count1;
	int i;
	struct axxia_edac_dev_info *dev_info;

	dev_info = edac_dev->pvt_info;

	val = read_l2merrsr();
	if (val & 0x80000000) {
		int cpu = get_cpu();

		count0 = ((val) & 0x000000ff00000000) >> 20;
		count1 = ((val) & 0x0000ff0000000000) >> 28;

		/* increment correctable error counts */
		for (i = 0; i < count0 + count1; i++) {
			edac_device_handle_ce(edac_dev, 0,
					      cpu / CORES_PER_CLUSTER,
					      edac_dev->ctl_name);
		}

		/* Clear the valid bit */
		clear_val = 0x80000000;
		write_l2merrsr(clear_val);
		put_cpu();
	}
	if (val & 0x8000000000000000) {
		regmap_update_bits(dev_info->syscon,
				   APB2_PERSIST_SCRATCH,
				   L2_PERSIST_SCRATCH_BIT,
				   L2_PERSIST_SCRATCH_BIT);
		pr_emerg("L2 uncorrectable error\n");
		machine_restart(NULL);
	}
}

/* Check for L2 Errors */
static void axxia_l2_error_check(struct edac_device_ctl_info *edac_dev)
{
	/* 4 cores per cluster */
	int nr_cluster_ids = ((nr_cpu_ids - 1) / CORES_PER_CLUSTER) + 1;
	int i, j, cpu;

	/* execute on current cpu */
	log_l2merrsr(edac_dev);

	for (i = 0; i < nr_cluster_ids; i++) {
		/* No need to run on local cluster. */
		if (i == (get_cpu() / CORES_PER_CLUSTER)) {
			put_cpu();
			continue;
		}
		/*
		 * Have some core in each cluster execute this,
		 * Start with the first core on that cluster.
		 */
		cpu = i * CORES_PER_CLUSTER;
		for (j = cpu; j < cpu + CORES_PER_CLUSTER; j++) {
			if (cpu_online(j)) {
				smp_call_function_single(j, log_l2merrsr,
							 edac_dev, 1);
				break;
			}
		}
		put_cpu();
	}
}

static void axxia_add_edac_devices(struct platform_device *pdev,
				   int num)
{
	struct axxia_edac_dev_info *dev_info = NULL;
	/* 4 cores per cluster */
	int nr_cluster_ids = ((nr_cpu_ids - 1) / CORES_PER_CLUSTER) + 1;
	struct device_node *np = pdev->dev.of_node;

	dev_info = devm_kzalloc(&pdev->dev, sizeof(*dev_info), GFP_KERNEL);
	if (!dev_info)
		return;

	dev_info->ctl_name = kstrdup(np->name, GFP_KERNEL);
	if (!dev_info->ctl_name) {
		devm_kfree(&pdev->dev, dev_info);
		return;
	}

	if (num == 0) {
		dev_info->blk_name = "cpumerrsr";
		dev_info->check = axxia_cpu_error_check;
	} else {
		dev_info->blk_name = "l2merrsr";
		dev_info->check = axxia_l2_error_check;
	}
	dev_info->pdev = pdev;
	dev_info->edac_idx = edac_device_alloc_index();
	dev_info->syscon =
			syscon_regmap_lookup_by_phandle(np, "syscon");
	if (IS_ERR(dev_info->syscon)) {
		pr_info("%s: syscon lookup failed\n", np->name);
		goto err1;
	}

	if (strcmp(dev_info->ctl_name, "edac_cpu") == 0) {
		dev_info->edac_dev =
		edac_device_alloc_ctl_info(0, dev_info->ctl_name,
					   1, dev_info->blk_name,
					   num_possible_cpus(), 0, NULL,
					   0, dev_info->edac_idx);
	} else if (strcmp(dev_info->ctl_name, "edac_l2") == 0) {
		dev_info->edac_dev =
		edac_device_alloc_ctl_info(0, dev_info->ctl_name,
					   1, dev_info->blk_name,
					   nr_cluster_ids, 0, NULL,
					   0, dev_info->edac_idx);
	}
	if (!dev_info->edac_dev) {
		pr_info("No memory for edac device\n");
		goto err1;
	}

	dev_info->edac_dev->pvt_info = dev_info;
	dev_info->edac_dev->dev = &dev_info->pdev->dev;
	dev_info->edac_dev->ctl_name = dev_info->ctl_name;
	dev_info->edac_dev->mod_name = AXXIA_EDAC_MOD_STR;
	dev_info->edac_dev->dev_name = dev_name(&dev_info->pdev->dev);

	dev_info->edac_dev->edac_check = dev_info->check;

	if (edac_device_add_device(dev_info->edac_dev) != 0) {
		pr_info("Unable to add edac device for %s\n",
			dev_info->ctl_name);
		goto err2;
	}

	return;
err2:
	edac_device_free_ctl_info(dev_info->edac_dev);
err1:
	platform_device_unregister(dev_info->pdev);
}

static int axxia_edac_cpu_probe(struct platform_device *pdev)
{
	edac_op_state = EDAC_OPSTATE_POLL;
	axxia_add_edac_devices(pdev, 0);
	return 0;
}

static int axxia_edac_cpu_remove(struct platform_device *pdev)
{
	platform_device_unregister(pdev);
	return 0;
}

static int axxia_edac_l2_probe(struct platform_device *pdev)
{
	edac_op_state = EDAC_OPSTATE_POLL;
	axxia_add_edac_devices(pdev, 1);
	return 0;
}

static int axxia_edac_l2_remove(struct platform_device *pdev)
{
	platform_device_unregister(pdev);
	return 0;
}

static const struct of_device_id axxia_edac_l2_match[] = {
	{
	.compatible = "axxia,cortex-a15-l2-cache",
	},
	{},
};

static struct platform_driver axxia_edac_l2_driver = {
	.probe = axxia_edac_l2_probe,
	.remove = axxia_edac_l2_remove,
	.driver = {
		.name = "axxia_edac_l2",
		.of_match_table = axxia_edac_l2_match,
	}
};

static const struct of_device_id axxia_edac_cpu_match[] = {
	{
	.compatible = "axxia,cortex-a15-cpu",
	},
	{},
};

static struct platform_driver axxia_edac_cpu_driver = {
	.probe = axxia_edac_cpu_probe,
	.remove = axxia_edac_cpu_remove,
	.driver = {
		.name = "axxia_edac_cpu",
		.of_match_table = axxia_edac_cpu_match,
	}
};

module_platform_driver(axxia_edac_cpu_driver);
module_platform_driver(axxia_edac_l2_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sangeetha Rao <sangeetha.rao@intel.com>");
