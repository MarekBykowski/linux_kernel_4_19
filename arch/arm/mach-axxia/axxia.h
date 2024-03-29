/* SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2003 INTEL Corporation
 */
#ifndef _AXXIA_H

void axxia_init_clocks(int is_sim);
void axxia_ddr_retention_init(void);
void axxia_platform_cpu_die(unsigned int cpu);
int axxia_platform_cpu_kill(unsigned int cpu);
void ncp_ddr_shutdown(void *a, void *b, unsigned long c);
void flush_l3(void);

void axxia_secondary_startup(void);

extern struct smp_operations axxia_smp_ops;

extern void __iomem *syscon;
extern void __iomem *dickens;

#endif
