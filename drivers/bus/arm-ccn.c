/*
 * CCN cache coherent interconnect driver
 *
 * Author: Marek Bykowski <marek.bykowski@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"ARM-CCN: " fmt

#include <linux/arm-ccn.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/perf_event.h>
#include <linux/of_irq.h>

#define CCN_MN_ERR_SIG_VAL_63_0		0x0300
#define CCN_MN_ERR_SIG_VAL_63_0__DT			(1 << 1)

#define CCN_NUM_REGIONS	256
#define CCN_MN_OLY_COMP_LIST_63_0	0x01e0

#define CCN_ALL_OLY_ID			0xff00
#define CCN_ALL_OLY_ID__OLY_ID__SHIFT			0
#define CCN_ALL_OLY_ID__OLY_ID__MASK			0x1f
#define CCN_ALL_OLY_ID__NODE_ID__SHIFT			8
#define CCN_ALL_OLY_ID__NODE_ID__MASK			0x3f

struct arm_ccn {
	struct device *dev;
	void __iomem *base;
	unsigned int irq;

	unsigned sbas_present:1;
	unsigned sbsx_present:1;

	int num_nodes;
	struct arm_ccn_component *node;

	int num_xps;
	struct arm_ccn_component *xp;

	struct arm_ccn_dt dt;
	int mn_id;
};

static struct ccn_ctrl *ccn_ctrl;

static int arm_ccn_init_nodes(struct arm_ccn *ccn, int region,
		void __iomem *base, u32 type, u32 id)
{
	struct arm_ccn_component *component;
	struct resource *res;

	pr_info("Region %d: id=%u, type=0x%02x\n", region, id, type);

	res = platform_get_resource(
			container_of(ccn->dev, struct platform_device, dev), IORESOURCE_MEM, 0);

	switch (type) {
	case CCN_TYPE_MN:
		ccn->mn_id = id;
		ccn_ctrl->mn_id = id;
		return 0;
	case CCN_TYPE_DT:
		return 0;
	case CCN_TYPE_XP:
		component = &ccn->xp[id];
		/* Get a phy addr of xp so that we populate to child/ren */
		ccn_ctrl->xp.base[id] = res->start + (phys_addr_t) (base - ccn->base);	
		pr_info("mb: ccn_ctrl->xp.base[id] %lx\n", (unsigned long) ccn_ctrl->xp.base[id]);
		break;
	case CCN_TYPE_SBSX:
		ccn->sbsx_present = 1;
		component = &ccn->node[id];
		/* Get a phy addr of node so that we populate to child/ren */
		ccn_ctrl->node.base[id] = res->start + (phys_addr_t) (base - ccn->base);	
		break;
	case CCN_TYPE_SBAS:
		ccn->sbas_present = 1;
		/* Fall-through */
	default:
		component = &ccn->node[id];
		/* Get a phy addr of node so that we populate to child/ren */
		ccn_ctrl->node.base[id] = res->start + (phys_addr_t) (base - ccn->base);	
		break;
	}

	component->base = base;
	component->type = type;

	return 0;
}

static int arm_ccn_get_nodes_num(struct arm_ccn *ccn, int region,
		void __iomem *base, u32 type, u32 id)
{
	if (type == CCN_TYPE_XP && id >= ccn->num_xps)
		ccn->num_xps = id + 1;
	else if (id >= ccn->num_nodes)
		ccn->num_nodes = id + 1;

	return 0;
}

static int arm_ccn_for_each_valid_region(struct arm_ccn *ccn,
		int (*callback)(struct arm_ccn *ccn, int region,
		void __iomem *base, u32 type, u32 id))
{
	int region;

	for (region = 0; region < CCN_NUM_REGIONS; region++) {
		u32 val, type, id;
		void __iomem *base;
		int err;

		val = readl(ccn->base + CCN_MN_OLY_COMP_LIST_63_0 +
				4 * (region / 32));
		if (!(val & (1 << (region % 32))))
			continue;

		base = ccn->base + region * CCN_REGION_SIZE;
		val = readl(base + CCN_ALL_OLY_ID);
		type = (val >> CCN_ALL_OLY_ID__OLY_ID__SHIFT) &
				CCN_ALL_OLY_ID__OLY_ID__MASK;
		id = (val >> CCN_ALL_OLY_ID__NODE_ID__SHIFT) &
				CCN_ALL_OLY_ID__NODE_ID__MASK;

		err = callback(ccn, region, base, type, id);
		if (err)
			return err;
	}

	return 0;
}

static const struct of_device_id arm_ccn_child_matches[] = {
	{.compatible = "arm,l3_edac", },
	{.compatible = "arm,ccn-504-pmu", },
	{.compatible = "arm,ccn-508-pmu", },
	{.compatible = "arm,ccn-512-pmu", },
	{},
};

#define NR_OF_INTERRUPTS 2

enum {
	ALL_ERRORS,
	PMU_OVERFLOW
};

struct error_reporting {
	char *match;
	bool poll;
	union {
		irqreturn_t (*pmu_overflow_handler) (struct arm_ccn_dt *dt);
		irqreturn_t (*error_handler) (struct arm_ccn *ccn,
				const u32 *err_sig_val);
	};
};

struct error_reporting error_reporting[NR_OF_INTERRUPTS] = {
	[ALL_ERRORS] = {
		/* Partial match looking for a substring 'edac' in 'compatible' */
		.match = "edac",
		.poll = false,
#ifndef CONFIG_ARM_CCN_L3_EDAC
		.error_handler = NULL /*to be defined */,
#endif
	},
	[PMU_OVERFLOW] = {
		.match = "pmu",
		.poll = false,
#ifndef CONFIG_ARM_CCN_PMU
		.pmu_overflow_handler = arm_ccn_pmu_overflow_handler,
#endif
	}
};

static irqreturn_t arm_ccn_irq_handler(int irq, void *dev_id)
{
	irqreturn_t res = IRQ_NONE;
	struct arm_ccn *ccn = dev_id;
	u32 err_sig_val[6];
	u32 err_or;
	int i;

	/* PMU overflow is a special case */
	err_or = err_sig_val[0] = readl(ccn->base + CCN_MN_ERR_SIG_VAL_63_0);
	if (err_or & CCN_MN_ERR_SIG_VAL_63_0__DT) {
		err_or &= ~CCN_MN_ERR_SIG_VAL_63_0__DT;
		if (error_reporting[PMU_OVERFLOW].pmu_overflow_handler) {
			res = error_reporting[PMU_OVERFLOW].pmu_overflow_handler(&ccn->dt);
		} else {
			pr_info("Disabling interrupt generation for PMU counters' overflow.\n");
			writel(CCN_MN_ERRINT_STATUS__PMU_EVENTS__DISABLED,
					ccn->base + CCN_MN_ERRINT_STATUS);
			res = IRQ_HANDLED;
		}
	}

	/* Have to read all err_sig_vals and clear them */
	for (i = 1; i < ARRAY_SIZE(err_sig_val); i++) {
		err_sig_val[i] = readl(ccn->base +
				CCN_MN_ERR_SIG_VAL_63_0 + i * 4);
		err_or |= err_sig_val[i];
	}
	if (err_or) {
		if (error_reporting[ALL_ERRORS].error_handler) {
			res |= error_reporting[ALL_ERRORS].error_handler(ccn, err_sig_val);
		} else {
			pr_info("Disabling interrupt generation for PMU counters' overflow.\n");
			writel(CCN_MN_ERRINT_STATUS__PMU_EVENTS__DISABLED,
					ccn->base + CCN_MN_ERRINT_STATUS);
			res |= IRQ_HANDLED;
		}
	}

	if (res != IRQ_NONE)
		writel(CCN_MN_ERRINT_STATUS__INTREQ__DESSERT,
				ccn->base + CCN_MN_ERRINT_STATUS);

	return res;
}

/* Figure out how to handle ccn errors and PMU counters' overflow */
static int ccn_set_error_reporting(struct device_node *np)
{
	struct device_node *cp;
	int i;

	for_each_available_child_of_node(np, cp) {
		pr_info("mb: lopping through child cp->name %s\n",
				cp->name);
		/* If none exists */
		if (!of_match_node(arm_ccn_child_matches, cp))
			continue;

		/* If 'status' is disabled */
		if (!of_device_is_available(cp))
			continue;

		/*
		 * If 'poll-mode' property is present it means the
		 * node requests the polling.
		 */
		if (of_get_property(cp, "poll-mode", NULL)) {
			for (i = 0; i < NR_OF_INTERRUPTS; i++)
				if (NULL != strstr(cp->name, error_reporting[i].match)) {
					error_reporting[i].poll = true;
				}
		}
	}

	return 0;
}

static const struct of_dev_auxdata arm_ccn_auxdata[] = {
	OF_DEV_AUXDATA("arm,ccn-502-pmu", 0, NULL, &ccn_ctrl),
	OF_DEV_AUXDATA("arm,ccn-504-pmu", 0, NULL, &ccn_ctrl),
	OF_DEV_AUXDATA("arm,ccn-512-pmu", 0, NULL, &ccn_ctrl),
	{}
};

static const struct of_device_id arm_ccn_matches[] = {
	{ .compatible = "arm,ccn-502", },
	{ .compatible = "arm,ccn-504", },
	{ .compatible = "arm,ccn-512", },
	{},
};
MODULE_DEVICE_TABLE(of, arm_ccn_matches);

static int ccn_platform_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *np;
#define INIT_BY_NODE 0
#if INIT_BY_NODE
	struct resource res;
#else
	struct resource *res;
#endif
	unsigned int irq = 0;
	struct arm_ccn *ccn;
	int err;

	ccn = devm_kzalloc(&pdev->dev, sizeof(*ccn), GFP_KERNEL);
	if (!ccn)
		return -ENOMEM;

	np = of_find_matching_node(NULL, arm_ccn_matches);
	if (!np)
		return -ENODEV;

#if INIT_BY_NODE
	ret = of_address_to_resource(np, 0, &res);
	if (!ret) {
		ccn->base = ioremap(res.start, resource_size(&res));
	}
	if (ret || !ccn->base) {
		WARN(1, "unable to ioremap CCN ctrl\n");
		return -ENXIO;
	}

	ret = of_irq_to_resource(np, 0, &res);
	if (!ret) {
		return -EINVAL;
	}
	irq = res.start;
#else
	ccn->dev = &pdev->dev;
	platform_set_drvdata(pdev, ccn);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ccn->base = devm_ioremap_resource(ccn->dev, res);
	if (IS_ERR(ccn->base))
		return PTR_ERR(ccn->base);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res)
		return -EINVAL;
	irq = res->start;
#endif

	(void) ccn_set_error_reporting(np);

	/* Check if we can use the interrupt */
	writel(CCN_MN_ERRINT_STATUS__PMU_EVENTS__DISABLE,
			ccn->base + CCN_MN_ERRINT_STATUS);
	if (readl(ccn->base + CCN_MN_ERRINT_STATUS) &
			CCN_MN_ERRINT_STATUS__PMU_EVENTS__DISABLED) {
		/* Can acknowledge the interrupt so can use it */
		writel(CCN_MN_ERRINT_STATUS__PMU_EVENTS__ENABLE,
				ccn->base + CCN_MN_ERRINT_STATUS);
		ret = request_irq(irq, arm_ccn_irq_handler,
				       IRQF_NOBALANCING | IRQF_NO_THREAD,
				       np->name, ccn);
		if (ret)
			return ret;

	}

	/* Build topology */
	err = arm_ccn_for_each_valid_region(ccn, arm_ccn_get_nodes_num);
	if (err)
		return err;

	ccn->node = devm_kcalloc(ccn->dev, ccn->num_nodes, sizeof(*ccn->node),
				 GFP_KERNEL);
	ccn->xp = devm_kcalloc(ccn->dev, ccn->num_xps, sizeof(*ccn->node),
			       GFP_KERNEL);
	if (!ccn->node || !ccn->xp)
		return -ENOMEM;

	ccn_ctrl = devm_kzalloc(ccn->dev, sizeof(*ccn_ctrl), GFP_KERNEL);
	if (!ccn_ctrl)
		return ENOMEM;

	ccn_ctrl->xp.base = devm_kcalloc(ccn->dev, ccn->num_xps,
			sizeof(phys_addr_t), GFP_KERNEL);

	ccn_ctrl->node.base = devm_kcalloc(ccn->dev, ccn->num_nodes,
			sizeof(phys_addr_t), GFP_KERNEL);
	if (!ccn_ctrl->xp.base || !ccn_ctrl->node.base)
		return -ENOMEM;

	err = arm_ccn_for_each_valid_region(ccn, arm_ccn_init_nodes);
	if (err)
		return err;

	/* Populate aux data for a child/ren */
	ccn_ctrl->irq = irq;
	ccn_ctrl->sbas_present = ccn->sbas_present;
	ccn_ctrl->sbsx_present = ccn->sbsx_present;
	ccn_ctrl->xp.num_xps = ccn->num_xps;
	ccn_ctrl->node.num_nodes = ccn->num_nodes;

{
	int i;
	for (i = 0; i< ccn_ctrl->xp.num_xps; i++)
		pr_info("mb: ccn_ctrl->xp.base[%d] %lx\n",
			i, (unsigned long)ccn_ctrl->xp.base[i]);
}

	ret = of_platform_populate(pdev->dev.of_node, NULL,
				    arm_ccn_auxdata, &pdev->dev);

	if (ret)
		dev_err(&pdev->dev,
			"failed to populate bus devices\n");

	pr_info("ARM CCN driver probed\n");

	return 0;
}

static struct platform_driver ccn_platform_driver = {
	.driver = {
		   .name = "arm-ccn",
		   .of_match_table = arm_ccn_matches,
		   .owner = THIS_MODULE,
		  },
	.probe = ccn_platform_probe,
};

static int __init ccn_platform_init(void)
{
	return platform_driver_register(&ccn_platform_driver);
}

core_initcall(ccn_platform_init);
MODULE_AUTHOR("Marek Bykowski <marek.bykowski@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ARM ccn support");
