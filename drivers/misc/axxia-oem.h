/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2020 INTEL */

#ifndef __AXXIA_KVM_EOM_H__
#define __AXXIA_KVM_EOM_H__

#define DMA_X_SRC_COUNT                                0x00
#define DMA_Y_SRC_COUNT                                0x04
#define DMA_X_MODIF_SRC                                0x08
#define DMA_Y_MODIF_SRC                                0x0c
#define DMA_SRC_CUR_ADDR                       0x10
#define DMA_SRC_ACCESS                         0x14
#define    DMA_SRC_ACCESS_TAIL_LENGTH(x)       (((x) & 0xf) << 11)
#define    DMA_SRC_ACCESS_SRC_MASK_LENGTH(x)   (((x) & 0x1f) << 6)
#define    DMA_SRC_ACCESS_SRC_SIZE(x)          (((x) & 7) << 3)
#define    DMA_SRC_ACCESS_SRC_BURST(x)         (((x) & 7) << 0)
#define DMA_SRC_MASK                           0x18
#define DMA_X_DST_COUNT                                0x1c
#define DMA_Y_DST_COUNT                                0x20
#define DMA_X_MODIF_DST                                0x24
#define DMA_DST_CUR_ADDR                       0x2c
#define DMA_DST_ACCESS                         0x30
#define    DMA_DST_ACCESS_DST_SIZE(x)          (((x) & 7) << 3)
#define    DMA_DST_ACCESS_DST_BURST(x)         (((x) & 7) << 0)
#define DMA_CHANNEL_CONFIG                     0x38
#define    DMA_CONFIG_DST_SPACE(x)             (((x) & 7) << 26)
#define    DMA_CONFIG_SRC_SPACE(x)             (((x) & 7) << 23)
#define    DMA_CONFIG_PRIORITY_ROW             (1<<21)
#define    DMA_CONFIG_PRIORITY                 (1<<20)
#define    DMA_CONFIG_CH_FULL_PRIORITY          (1<<19)
#define    DMA_CONFIG_LAST_BLOCK               (1<<15)
#define    DMA_CONFIG_CLEAR_FIFO               (1<<14)
#define    DMA_CONFIG_START_MEM_LOAD           (1<<13)
#define    DMA_CONFIG_STOP_DST_EOB             (1<<11)
#define    DMA_CONFIG_FULL_DESCR_ADDR          (1<<8)
#define    DMA_CONFIG_INT_DST_EOT              (1<<7)
#define    DMA_CONFIG_INT_DST_EOB              (1<<6)
#define    DMA_CONFIG_WAIT_FOR_TASK_CNT2       (1<<5)
#define    DMA_CONFIG_TASK_CNT2_RESET          (1<<4)
#define    DMA_CONFIG_WAIT_FOR_TASK_CNT1       (1<<3)
#define    DMA_CONFIG_TASK_CNT1_RESET          (1<<2)
#define    DMA_CONFIG_TX_EN                    (1<<1)
#define    DMA_CONFIG_CHAN_EN                  (1<<0)
#define DMA_STATUS                             0x3c
#define    DMA_STATUS_WAIT_TASK_CNT2           (1<<20)
#define    DMA_STATUS_TASK_CNT2_OVERFLOW       (1<<19)
#define    DMA_STATUS_WAIT_TASK_CNT1           (1<<18)
#define    DMA_STATUS_TASK_CNT1_OVERFLOW       (1<<17)
#define    DMA_STATUS_CH_PAUS_WR_EN            (1<<16)
#define    DMA_STATUS_ERR_ACC_DESCR            (1<<14)
#define    DMA_STATUS_ERR_ACC_DST              (1<<13)
#define    DMA_STATUS_ERR_ACC_SRC              (1<<12)
#define    DMA_STATUS_ERR_OVERFLOW             (1<<9)
#define    DMA_STATUS_ERR_UNDERFLOW            (1<<8)
#define    DMA_STATUS_CH_PAUSE                 (1<<7)
#define    DMA_STATUS_CH_WAITING               (1<<5)
#define    DMA_STATUS_CH_ACTIVE                        (1<<4)
#define    DMA_STATUS_TR_COMPLETE              (1<<3)
#define    DMA_STATUS_BLK_COMPLETE             (1<<2)
#define    DMA_STATUS_UNALIGNED_READ           (1<<1)
#define    DMA_STATUS_UNALIGNED_WRITE          (1<<0)
#define    DMA_STATUS_UNALIGNED_ERR            (DMA_STATUS_UNALIGNED_READ | \
						DMA_STATUS_UNALIGNED_WRITE)
#define DMA_SRC_ADDR_SEG                       0x54
#define DMA_DST_ADDR_SEG                       0x58

#endif /* __AXXIA_KVM_EOM_H__ */
