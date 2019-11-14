/* SPDX-License-Identifier: GPL-2.0 */
/*
 * include/linux/arm-ccn.h
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

#ifndef __LINUX_ARM_CCN_H
#define __LINUX_ARM_CCN_H

#include <linux/list.h>
#include <linux/irqreturn.h>
#include <asm/arm-ccn.h>

struct error_reporting {
	const char *match;
	void *data;
	irqreturn_t (*handler)(void *data);
	struct list_head child;
};

#if defined(CONFIG_ARM_CCN)
void arm_ccn_handler_add(struct list_head *new);
#else
void arm_ccn_handler_add(struct list_head *new)
{ return; }
#endif

#endif
