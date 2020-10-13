// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 INTEL

#include <linux/types.h>
#include <asm/kvm_arm.h>
#include <asm/kvm_asm.h>
#include <asm/kvm_hyp.h>
#include <asm/kvm_mmu.h>
#include <linux/delay.h>

#include "../../../../drivers/misc/axxia-oem.h"

u32 __hyp_text __enable_ccn_access(unsigned int phys_addr)
{
	unsigned int gpdma0_axprot_override;
	unsigned int gpdma0_status;
	int retries = 1000;

	/* Update CCN 0, bit 0 */

	/* Make sure no other transactions are in process */
	gpdma0_status = readl(kern_hyp_va(gpdma0 + DMA_STATUS));
	if (0 != (gpdma0_status & DMA_STATUS_CH_ACTIVE))
		return -1;

	/* Clear status bits. */
	writel((DMA_STATUS_TR_COMPLETE | DMA_STATUS_BLK_COMPLETE),
		kern_hyp_va(gpdma0 + DMA_STATUS));

	/* Set gpdma0_axprot_override to secure or non-secure */
	gpdma0_axprot_override = readl(kern_hyp_va(mmap_scb + 0x48800));
	writel(2, kern_hyp_va(mmap_scb + 0x48800));

	/* Destination is 0x80_0000_0000 */
	writel(0x80, kern_hyp_va(gpdma0 + DMA_DST_ADDR_SEG));
	writel(0, kern_hyp_va(gpdma0 + DMA_DST_CUR_ADDR));

	/* phys_addr is the source */
	writel((unsigned long)((phys_addr & 0xff00000000) >> 32),
	       kern_hyp_va(gpdma0 + DMA_SRC_ADDR_SEG));
	writel((unsigned long)(phys_addr & 0xffffffff),
	       kern_hyp_va(gpdma0 + DMA_SRC_CUR_ADDR));

	/* Remaing setup. */
	writel(DMA_SRC_ACCESS_SRC_MASK_LENGTH(8) |
	       DMA_SRC_ACCESS_SRC_SIZE(3) |
	       DMA_SRC_ACCESS_SRC_BURST(5),
	       kern_hyp_va(gpdma0 + DMA_SRC_ACCESS));
	writel(DMA_DST_ACCESS_DST_SIZE(3) |
	       DMA_DST_ACCESS_DST_BURST(5),
	       kern_hyp_va(gpdma0 + DMA_DST_ACCESS));
	writel(0xffffffff, kern_hyp_va(gpdma0 + DMA_SRC_MASK));
	writel(sizeof(unsigned long), kern_hyp_va(gpdma0 + DMA_X_MODIF_SRC));
	writel(sizeof(unsigned long), kern_hyp_va(gpdma0 + DMA_X_MODIF_DST));
	writel(1, kern_hyp_va(gpdma0 + DMA_X_SRC_COUNT));
	writel(0, kern_hyp_va(gpdma0 + DMA_Y_SRC_COUNT));
	writel(1, kern_hyp_va(gpdma0 + DMA_X_DST_COUNT));
	writel(0, kern_hyp_va(gpdma0 + DMA_Y_DST_COUNT));

	/* Start the transfer. */
	writel(DMA_CONFIG_DST_SPACE(1) |
	       DMA_CONFIG_SRC_SPACE(1) |
	       DMA_CONFIG_CH_FULL_PRIORITY |
	       DMA_CONFIG_LAST_BLOCK |
	       DMA_CONFIG_FULL_DESCR_ADDR |
	       DMA_CONFIG_WAIT_FOR_TASK_CNT2 |
	       DMA_CONFIG_WAIT_FOR_TASK_CNT1 |
	       DMA_CONFIG_TX_EN |
	       DMA_CONFIG_CHAN_EN, kern_hyp_va(gpdma0 + DMA_CHANNEL_CONFIG));

	/* Wait for completion. */
	while (0 < retries--) {
		unsigned long __loop = 1000;

		if (0 != (readl(kern_hyp_va(gpdma0 + DMA_STATUS)) & 0x8) &&
		    0 == readl(kern_hyp_va(gpdma0 + DMA_X_DST_COUNT)))
			break;

		/* No delay/arm timer/s as such use prosthesis */
		__asm__ volatile(
		"	mov x9, %0\n"
		"	1: subs x9, x9, #1\n"
		"	b.ne 1b"
		::"r"(__loop));
	}

	return 0;
	/* Restore gpdma0_axprot_override. */
	writel(gpdma0_axprot_override, kern_hyp_va(mmap_scb + 0x48800));

	if (0 >= retries)
		return -EFAULT;

	return 0;
}
