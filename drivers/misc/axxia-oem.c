// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 INTEL

/*
 * ===========================================================================
 * ===========================================================================
 * Private
 * ===========================================================================
 * ===========================================================================
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/linkage.h>
#include <linux/axxia-oem.h>
#include <linux/uaccess.h>
#include <linux/arm-smccc.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <asm/cacheflush.h>

/*
 * DSP Cluster Control
 */

static ssize_t
axxia_dspc_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
	static int finished;
	char return_buffer[80];

	if (finished) {
		finished = 0;

		return 0;
	}

	finished = 1;
	sprintf(return_buffer, "0x%lx\n", axxia_dspc_get_state());

	if (copy_to_user(buffer, return_buffer, strlen(return_buffer)))
		return -EFAULT;

	return strlen(return_buffer);
}

static ssize_t
axxia_dspc_write(struct file *file, const char __user *buffer,
		 size_t count, loff_t *ppos)
{
	char *input;
	int rc;
	unsigned long res;

	input = kmalloc(count + 1, GFP_KERNEL);

	if (!input)
		return -ENOSPC;

	if (copy_from_user(input, buffer, count)) {
		kfree(input);

		return -EFAULT;
	}

	input[count] = 0;
	rc = kstrtoul(input, 0, &res);

	if (rc) {
		kfree(input);

		return rc;
	}

	axxia_dspc_set_state(res);
	kfree(input);

	return count;
}

static const struct file_operations axxia_dspc_proc_ops = {
	.read       = axxia_dspc_read,
	.write      = axxia_dspc_write,
	.llseek     = noop_llseek,
};

/*
 * ACTLR_EL3 Control
 */

static ssize_t
axxia_actlr_el3_read(struct file *filp, char *buffer, size_t length,
		     loff_t *offset)
{
	static int finished;
	char return_buffer[80];

	if (finished != 0) {
		finished = 0;

		return 0;
	}

	finished = 1;
	sprintf(return_buffer, "0x%lx\n", axxia_actlr_el3_get());

	if (copy_to_user(buffer, return_buffer, strlen(return_buffer)))
		return -EFAULT;

	return strlen(return_buffer);
}

static ssize_t
axxia_actlr_el3_write(struct file *file, const char __user *buffer,
		      size_t count, loff_t *ppos)
{
	char *input;
	int rc;
	unsigned long res;

	input = kmalloc(count + 1, __GFP_RECLAIMABLE);

	if (!input)
		return -ENOSPC;

	if (copy_from_user(input, buffer, count)) {
		kfree(input);

		return -EFAULT;
	}

	input[count] = 0;
	rc = kstrtoul(input, 0, &res);

	if (rc) {
		kfree(input);

		return rc;
	}

	axxia_actlr_el3_set(res);
	kfree(input);

	return count;
}

static const struct file_operations axxia_actlr_el3_proc_ops = {
	.read       = axxia_actlr_el3_read,
	.write      = axxia_actlr_el3_write,
	.llseek     = noop_llseek,
};

/*
 * ACTLR_EL2 Control
 */

static ssize_t
axxia_actlr_el2_read(struct file *filp, char *buffer, size_t length,
		     loff_t *offset)
{
	static int finished;
	char return_buffer[80];

	if (finished != 0) {
		finished = 0;

		return 0;
	}

	finished = 1;
	sprintf(return_buffer, "0x%lx\n", axxia_actlr_el2_get());

	if (copy_to_user(buffer, return_buffer, strlen(return_buffer)))
		return -EFAULT;

	return strlen(return_buffer);
}

static ssize_t
axxia_actlr_el2_write(struct file *file, const char __user *buffer,
		      size_t count, loff_t *ppos)
{
	char *input;
	int rc;
	unsigned long res;

	input = kmalloc(count + 1, __GFP_RECLAIMABLE);

	if (!input)
		return -ENOSPC;

	if (copy_from_user(input, buffer, count)) {
		kfree(input);

		return -EFAULT;
	}

	input[count] = 0;
	rc = kstrtoul(input, 0, &res);

	if (rc) {
		kfree(input);

		return rc;
	}

	axxia_actlr_el2_set(res);
	kfree(input);

	return count;
}

static const struct file_operations axxia_actlr_el2_proc_ops = {
	.read       = axxia_actlr_el2_read,
	.write      = axxia_actlr_el2_write,
	.llseek     = noop_llseek,
};

/*
 * CCN Access
 */

#include <linux/kvm_host.h>
#include <asm/kvm_hyp.h>
#include <asm/kvm_asm.h>
#include <asm/kvm_arm.h>
#include <asm/kvm_mmu.h>
#include <linux/string.h> /* for macro */
#include "axxia-oem.h"

/* Helper macro */
#define is_gpdma_success(str)						\
	({								\
		__label__ __out;					\
		void __iomem *__ccn_sec = NULL;				\
		bool __is_suc = false;					\
		unsigned long __value;					\
									\
		if (of_find_compatible_node(NULL, NULL,			\
					    "axxia,axc6732"))		\
			goto __out;					\
		else if (of_find_compatible_node(NULL, NULL,		\
						 "axxia,axm5616"))	\
			__ccn_sec = ioremap(0x8000000000, PAGE_SIZE);	\
		__value = readl(__ccn_sec);				\
		if (1 == (__value & 1)) {				\
			__is_suc = true;				\
			if (0 == strcmp(__stringify(str), "dbg"))	\
				pr_info("GPDMA0: wrote %lx @CCN.secure_access\n", \
					__value);			\
		}							\
									\
		iounmap(__ccn_sec);					\
__out:									\
		__is_suc;						\
	})

static int ccn_oem_available = 1;
static unsigned long long ccn_phys_base;

/* Used in EL2 so needs be global */
void __iomem *gpdma0;
void __iomem *mmap_scb;

static unsigned int *value;
static bool force_via_gpdma;
static bool run_at_el2;

int map_periph_el1(void)
{
	if (of_find_compatible_node(NULL, NULL, "axxia,axc6732")) {
		gpdma0 = ioremap(0x8005020000, SZ_64K);
	} else if (of_find_compatible_node(NULL, NULL, "axxia,axm5616")) {
		gpdma0 = ioremap(0x8004120000, SZ_64K);
	} else {
		pr_err("Only Valid for Axxia 6700 and 5600!\n");
	}
	if (!gpdma0)
		return -EIO;

	mmap_scb = ioremap(0x8032000000, SZ_512K);
	if (!mmap_scb)
		return -EIO;

	value = kmalloc(sizeof(unsigned long), GFP_ATOMIC);
	if (!value)
		return -ENOMEM;

	*value = 1;
	pr_debug("GPDMA0: mem write %u @0x%lx (PA) to be DMA'ed to CCN.sec\n",
		 *value, (unsigned long)virt_to_phys(value));

	return 0;
}

int map_periph_el2(void)
{
	unsigned int ret;

	ret = create_hyp_mappings((void *)gpdma0,
			(void *)(gpdma0 + SZ_64K), PAGE_HYP_DEVICE);
	if (ret)
		return ret;

	ret = create_hyp_mappings((void *)mmap_scb,
			(void *)(mmap_scb + SZ_512K), PAGE_HYP_DEVICE);
	if (ret)
		return ret;

	ret = create_hyp_mappings((void *)value,
			(void *)(value + 1), PAGE_HYP);
	if (ret)
		return ret;

	kvm_flush_dcache_to_poc(value, PAGE_SIZE);

	return ret;
}

void unmap_periph(void)
{
	if (gpdma0)
		iounmap(gpdma0);
	if (mmap_scb)
		iounmap(mmap_scb);
	kfree(value);
}

static int
enable_ccn_access(void)
{
	unsigned long *value;
	unsigned int gpdma0_axprot_override, gpdma0_offset;
	unsigned int gpdma0_status;
	int retries = 1000;

	/*
	 * Update CCN 0, bit 0
	 */

	value = kmalloc(sizeof(unsigned long), GFP_ATOMIC);

	if (!value)
		return -ENOMEM;

	*value = 1;
	__flush_dcache_area(value, sizeof(unsigned long));
	mb();

	/* Make sure no other transactions are in process. */
	gpdma0_status = readl(gpdma0 + DMA_STATUS);

	if (0 != (gpdma0_status & DMA_STATUS_CH_ACTIVE)) {
		pr_err("Error Reading DMA_STATUS!\n");
		return -1;
	}

	/* Clear status bits. */
	writel((DMA_STATUS_TR_COMPLETE | DMA_STATUS_BLK_COMPLETE),
               gpdma0 + DMA_STATUS);

	if (of_find_compatible_node(NULL, NULL, "axxia,axc6732")) {
		gpdma0_offset = 0x45800;
	} else if (of_find_compatible_node(NULL, NULL, "axxia,axm5616")) {
		gpdma0_offset = 0x48800;
	} else {
		pr_err("Internal Error!\n");
		return -EIO;
	}

	/* Set gpdma0_axprot_override to secure or non-secure. */
	gpdma0_axprot_override = readl(mmap_scb + gpdma0_offset);
	writel(2, mmap_scb + gpdma0_offset);

	/* Destination is 0x80_0000_0000 */
	writel(0x80, (gpdma0 + DMA_DST_ADDR_SEG));
	writel(0, (gpdma0 + DMA_DST_CUR_ADDR));

	/* value is the source */
	writel((unsigned long)((virt_to_phys(value) & 0xff00000000) >> 32),
	       gpdma0 + DMA_SRC_ADDR_SEG);
	writel((unsigned long)((virt_to_phys(value) & 0xffffffff)),
	       gpdma0 + DMA_SRC_CUR_ADDR);

	/* Remaing setup. */
	writel(DMA_SRC_ACCESS_SRC_MASK_LENGTH(8) |
	       DMA_SRC_ACCESS_SRC_SIZE(3) |
	       DMA_SRC_ACCESS_SRC_BURST(5),
	       gpdma0 + DMA_SRC_ACCESS);
	writel(DMA_DST_ACCESS_DST_SIZE(3) |
	       DMA_DST_ACCESS_DST_BURST(5),
	       gpdma0 + DMA_DST_ACCESS);
	writel(0xffffffff, gpdma0 + DMA_SRC_MASK);
	writel(sizeof(unsigned long), gpdma0 + DMA_X_MODIF_SRC);
	writel(sizeof(unsigned long), gpdma0 + DMA_X_MODIF_DST);
	writel(1, gpdma0 + DMA_X_SRC_COUNT);
	writel(0, gpdma0 + DMA_Y_SRC_COUNT);
	writel(1, gpdma0 + DMA_X_DST_COUNT);
	writel(0, gpdma0 + DMA_Y_DST_COUNT);

	/* Start the transfer. */
	writel(DMA_CONFIG_DST_SPACE(1) |
	       DMA_CONFIG_SRC_SPACE(1) |
	       DMA_CONFIG_CH_FULL_PRIORITY |
	       DMA_CONFIG_LAST_BLOCK |
	       DMA_CONFIG_FULL_DESCR_ADDR |
	       DMA_CONFIG_WAIT_FOR_TASK_CNT2 |
	       DMA_CONFIG_WAIT_FOR_TASK_CNT1 |
	       DMA_CONFIG_TX_EN |
	       DMA_CONFIG_CHAN_EN, gpdma0 + DMA_CHANNEL_CONFIG);

	/* Wait for completion. */
	while (0 < retries--) {
		if (0 != (readl(gpdma0 + DMA_STATUS) & 0x8) &&
		    0 == readl(gpdma0 + DMA_X_DST_COUNT))
			break;

		udelay(1);
	}

	/* Restore gpdma0_axprot_override. */
	writel(gpdma0_axprot_override, mmap_scb + gpdma0_offset);

	if (0 >= retries) {
		pr_err("gpdma_xfer timed out\n");
		return -EFAULT;
	}

	return 0;
}

static unsigned long ccn_offset;

static ssize_t
axxia_ccn_offset_read(struct file *filp, char *buffer, size_t length,
		      loff_t *offset)
{
	static int finished;
	char return_buffer[80];

	if (finished != 0) {
		finished = 0;

		return 0;
	}

	finished = 1;
	sprintf(return_buffer, "0x%lx\n", ccn_offset);

	if (copy_to_user(buffer, return_buffer, strlen(return_buffer)))
		return -EFAULT;

	return strlen(return_buffer);
}

static ssize_t
axxia_ccn_offset_write(struct file *file, const char __user *buffer,
		       size_t count, loff_t *ppos)
{
	char *input;
	unsigned int new_ccn_offset;
	int rc;
	unsigned long res;

	if (!buffer)
		return -EINVAL;

	input = kmalloc(count + 1, GFP_KERNEL);

	if (!input)
		return -ENOSPC;

	rc = copy_from_user(input, buffer, count);

	if (rc) {
		kfree(input);

		return -EFAULT;
	}

	input[count] = 0;
	rc = kstrtoul(input, 0, &res);

	if (rc) {
		kfree(input);

		return rc;
	}

	new_ccn_offset = (unsigned int)res;

	if (of_find_compatible_node(NULL, NULL, "axxia,axc6732")) {
		if (new_ccn_offset > 0x9cff00)
			pr_err("Invalid CCN Offset!\n");
	} else if (of_find_compatible_node(NULL, NULL, "axxia,axm5616")) {
		if (new_ccn_offset > 0x94ff00)
			pr_err("Invalid CCN Offset!\n");
	} else {
		pr_err("Internal Error!\n");
	}

	ccn_offset = (unsigned int)new_ccn_offset;
	kfree(input);

	return count;
}

static const struct file_operations axxia_ccn_offset_proc_ops = {
	.read       = axxia_ccn_offset_read,
	.write      = axxia_ccn_offset_write,
	.llseek     = noop_llseek,
};

static ssize_t
axxia_ccn_value_read(struct file *filp, char *buffer, size_t length,
		     loff_t *offset)
{
	static int finished;
	char return_buffer[80];

	if (finished != 0) {
		finished = 0;

		return 0;
	}

	finished = 1;
	sprintf(return_buffer, "0x%lx\n", axxia_ccn_get(ccn_offset));

	if (copy_to_user(buffer, return_buffer, strlen(return_buffer)))
		return -EFAULT;

	return strlen(return_buffer);
}

static ssize_t
axxia_ccn_value_write(struct file *file, const char __user *buffer,
		      size_t count, loff_t *ppos)
{
	char *input;
	int rc;
	unsigned long res;

	input = kmalloc(count + 1, __GFP_RECLAIMABLE);

	if (!input)
		return -ENOSPC;

	if (copy_from_user(input, buffer, count)) {
		kfree(input);

		return -EFAULT;
	}

	input[count] = 0;
	rc = kstrtoul(input, 0, &res);

	if (rc) {
		kfree(input);

		return rc;
	}

	axxia_ccn_set(ccn_offset, res);
	kfree(input);

	return count;
}

static const struct file_operations axxia_ccn_value_proc_ops = {
	.read       = axxia_ccn_value_read,
	.write      = axxia_ccn_value_write,
	.llseek     = noop_llseek,
};

/*
 * -----------------------------------------------------------------------------
 * setup_ccn_proc
 */

static int
setup_ccn_proc(void)
{
	int rc = 0;

	if (of_find_compatible_node(NULL, NULL, "axxia,axc6732")) {
		ccn_phys_base = 0x4000000000ULL;
	} else if (of_find_compatible_node(NULL, NULL, "axxia,axm5616")) {
		ccn_phys_base = 0x8000000000ULL;
	} else {
		pr_err("Only Valid for Axxia 6700 and 5600!\n");

		return -EFAULT;
	}

	ccn_offset = 0;

	if (proc_create("driver/axxia_ccn_offset", 0200, NULL,
			&axxia_ccn_offset_proc_ops) == NULL) {
		pr_err("Could not create /proc/driver/axxia_ccn_offset!\n");
		rc = -EFAULT;
	} else if (proc_create("driver/axxia_ccn_value", 0200, NULL,
			       &axxia_ccn_value_proc_ops) == NULL) {
		pr_err("Could not create /proc/driver/axxia_ccn_value!\n");
		rc = -EFAULT;
	}

	if (rc) {
		pr_err("Axxia CCN Access is Not Available!\n");
	} else {
		if (ccn_oem_available)
			pr_info("Axxia CCN (scm) Initialized\n");
		else
			pr_info("Axxia CCN (direct) Initialized\n");
	}

	return rc;
}

/*
 * ===========================================================================
 * ===========================================================================
 * Public
 * ===========================================================================
 * ===========================================================================
 */

/*
 * ---------------------------------------------------------------------------
 * axxia_dspc_get_state
 */

unsigned long
axxia_dspc_get_state(void)
{
	struct arm_smccc_res res;

	__arm_smccc_smc(0xc3000000, 0, 0, 0, 0, 0, 0, 0, &res, NULL);

	if (0 != res.a0)
		pr_warn("Getting the DSP State Failed!\n");

	return res.a1;
}
EXPORT_SYMBOL(axxia_dspc_get_state);

/*
 * ---------------------------------------------------------------------------
 * axxia_dspc_set_state
 */

void
axxia_dspc_set_state(unsigned long state)
{
	struct arm_smccc_res res;

	__arm_smccc_smc(0xc3000001, state, 0, 0, 0, 0, 0, 0, &res, NULL);

	if (0 != res.a0)
		pr_warn("Getting the DSP State Failed!\n");

	return;
}
EXPORT_SYMBOL(axxia_dspc_set_state);

/*
 * ---------------------------------------------------------------------------
 * axxia_actlr_el3_get
 */

unsigned long
axxia_actlr_el3_get(void)
{
	struct arm_smccc_res res;

	__arm_smccc_smc(0xc3000002, 0, 0, 0, 0, 0, 0, 0, &res, NULL);

	if (0 != res.a0)
		pr_warn("Getting ACTLR_EL3 Failed!\n");

	return res.a1;
}
EXPORT_SYMBOL(axxia_actlr_el3_get);

/*
 * ---------------------------------------------------------------------------
 * axxia_actlr_el3_set
 */

void
axxia_actlr_el3_set(unsigned long input)
{
	struct arm_smccc_res res;

	__arm_smccc_smc(0xc3000003, input, 0, 0, 0, 0, 0, 0, &res, NULL);

	if (0 != res.a0)
		pr_warn("Setting ACTLR_EL3 Failed!\n");

	return;
}
EXPORT_SYMBOL(axxia_actlr_el3_set);

/*
 * ---------------------------------------------------------------------------
 * axxia_actlr_el2_get
 */

unsigned long
axxia_actlr_el2_get(void)
{
	struct arm_smccc_res res;

	__arm_smccc_smc(0xc3000004, 0, 0, 0, 0, 0, 0, 0, &res, NULL);

	if (0 != res.a0)
		pr_warn("Getting ACTLR_EL2 Failed!\n");

	return res.a1;
}
EXPORT_SYMBOL(axxia_actlr_el2_get);

/*
 * ---------------------------------------------------------------------------
 * axxia_actlr_el2_set
 */

void
axxia_actlr_el2_set(unsigned long input)
{
	struct arm_smccc_res res;

	__arm_smccc_smc(0xc3000005, input, 0, 0, 0, 0, 0, 0, &res, NULL);

	if (0 != res.a0)
		pr_warn("Setting ACTLR_EL2 Failed!\n");

	return;
}
EXPORT_SYMBOL(axxia_actlr_el2_set);

/*
 * ---------------------------------------------------------------------------
 * axxia_ccn_get
 */

unsigned long
axxia_ccn_get(unsigned int offset)
{
	struct arm_smccc_res res;

	if (0 == ccn_oem_available) {
		void __iomem *ccn;

		ccn = ioremap(ccn_phys_base +
			      ((offset / SZ_1K) * SZ_1K), SZ_1K);
		res.a1 = readq(ccn + (offset % SZ_1K));
		iounmap(ccn);
	} else {
		__arm_smccc_smc(0xc3000006, offset, 0, 0, 0, 0, 0, 0,
				&res, NULL);

		if (0 != res.a0)
			pr_warn("Getting CCN Register Failed!\n");
	}

	return res.a1;
}
EXPORT_SYMBOL(axxia_ccn_get);

/*
 * ---------------------------------------------------------------------------
 * axxia_ccn_set
 */

void
axxia_ccn_set(unsigned int offset, unsigned long value)
{
	struct arm_smccc_res res;

	if (0 == ccn_oem_available) {
		void __iomem *ccn;

		ccn = ioremap(ccn_phys_base +
			      ((offset / SZ_1K) * SZ_1K), SZ_1K);
		writeq(value, (ccn + (offset % SZ_1K)));
		iounmap(ccn);
	} else {
		__arm_smccc_smc(0xc3000007, offset, value, 0, 0, 0, 0, 0,
				&res, NULL);

		if (0 != res.a0)
			pr_warn("Getting CCN Register Failed!\n");
	}

	return;
}
EXPORT_SYMBOL(axxia_ccn_set);

/*
 * ---------------------------------------------------------------------------
 * axxia_oem_init
 */

static int
axxia_oem_init(void)
{
	struct arm_smccc_res res;
	int rc = 0;

	if (of_find_compatible_node(NULL, NULL, "axxia,axc6732")) {
		/* Only applicable to the 6700. */
		if (proc_create("driver/axxia_dspc", 0200, NULL,
				&axxia_dspc_proc_ops) == NULL)
			pr_err("Could not create /proc/driver/axxia_dspc!\n");
		else
			pr_info("Axxia DSP Control Initialized\n");
	}

	if (proc_create("driver/axxia_actlr_el3", 0200, NULL,
			&axxia_actlr_el3_proc_ops) == NULL)
		pr_err("Could not create /proc/driver/axxia_actlr_el3!\n");
	else
		pr_info("Axxia ACTLR_EL3 Control Initialized\n");

	if (proc_create("driver/axxia_actlr_el2", 0200, NULL,
			&axxia_actlr_el2_proc_ops) == NULL)
		pr_err("Could not create /proc/driver/axxia_actlr_el2!\n");
	else
		pr_info("Axxia ACTLR_EL3 Control Initialized\n");

	/*
	  In some cases, CCN access is required (SBB work around for
	  example) but OEM access is not available (version of ATF
	  before 1.38).  As it happens, in U-Boot versions prior to
	  1.79, the hni_axprot_override is set.  This allows the
	  cores, while non-secure, to access the SCB blocks.  The
	  following provides CCN acces to non-secure masters by doing
	  the following:

	  -1-
	  Check for OEM access.  If available, skip the reset.

	  -2-
	  If OEM access is not avialable, try to enable non-secure
	  access by setting bit 0 of CCN offset 0 (the 'secure_acces'
	  bit).  See enable_ccn_access() above for details.

	  -3-
	  If #2 unseccessful, try #2 but at EL2/Hyp execution level
	  as opssosed to EL1 the Linux kernel runs at.
	*/

	__arm_smccc_smc(0xc3000006, 0xfe0, 0, 0, 0, 0, 0, 0, &res, NULL);

	if (0 != res.a0 || true == force_via_gpdma) {
		if (of_find_compatible_node(NULL, NULL, "axxia,axc6732")) {
			rc = -EPERM;
			goto out;
		}

		ccn_oem_available = 0;

		rc = map_periph_el1();
		if (rc) {
			pr_err("Cannot map GPDMA and/or SCB in EL1\n");
			goto out;
		}

		/*
		 * Try first EL1, if failure EL2.
		 * 'run_at_el2 = true' forces EL2.
		 */

		if (false == run_at_el2) {
			rc = enable_ccn_access();
			if (!is_gpdma_success()) {
				run_at_el2 = true;
				rc = map_periph_el2();
				if (rc) {
					pr_err("Cannot map GPDMA and/or SCB in EL2\n");
					goto out;
				}
			}
		} else {
			rc = map_periph_el2();
			if (rc) {
				pr_err("Cannot map GPDMA and/or SCB in EL2\n");
				goto out;
			}
		}

		if (run_at_el2) {
			preempt_disable();
			kvm_arch_hardware_enable();
			rc = kvm_call_hyp(__enable_ccn_access,
					  virt_to_phys(value));
			kvm_arch_hardware_disable();
			preempt_enable();
		}

out:
		unmap_periph();

		if (is_gpdma_success()) {
			pr_info("Successfully Enabled CCN Access via GPDMA at %s\n",
				false == run_at_el2 ? "EL1" : "EL2");
		} else if (!rc) {
			pr_info("Successfully ran 'GPDMA' WorkAround at %s ",
				false == run_at_el2 ? "EL1" : "EL2");
			pr_cont("but wasn't able to Enable Access!\n");
		} else {
			pr_err("Unable to Enable CCN Access via GPDMA!\n");
		}
	}

	if (!rc)
		rc = setup_ccn_proc();

	return rc;
}

device_initcall(axxia_oem_init);

MODULE_AUTHOR("John Jacques <john.jacques@intel.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Axxia OEM Control");
