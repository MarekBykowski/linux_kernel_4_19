/*
 * CCN cache coherent interconnect support
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __LINUX_ARM_CCN_H
#define __LINUX_ARM_CCN_H

#include <linux/io.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/perf_event.h>

#define CCN_MN_ERRINT_STATUS		0x0008
#define CCN_MN_ERRINT_STATUS__INTREQ__DESSERT		0x11
#define CCN_MN_ERRINT_STATUS__ALL_ERRORS__ENABLE	0x02
#define CCN_MN_ERRINT_STATUS__ALL_ERRORS__DISABLED	0x20
#define CCN_MN_ERRINT_STATUS__ALL_ERRORS__DISABLE	0x22
#define CCN_MN_ERRINT_STATUS__CORRECTED_ERRORS_ENABLE	0x04
#define CCN_MN_ERRINT_STATUS__CORRECTED_ERRORS_DISABLED	0x40
#define CCN_MN_ERRINT_STATUS__CORRECTED_ERRORS_DISABLE	0x44
#define CCN_MN_ERRINT_STATUS__PMU_EVENTS__ENABLE	0x08
#define CCN_MN_ERRINT_STATUS__PMU_EVENTS__DISABLED	0x80
#define CCN_MN_ERRINT_STATUS__PMU_EVENTS__DISABLE	0x88

#define CCN_TYPE_MN	0x01
#define CCN_TYPE_DT	0x02
#define CCN_TYPE_HNF	0x04
#define CCN_TYPE_HNI	0x05
#define CCN_TYPE_XP	0x08
#define CCN_TYPE_SBSX	0x0c
#define CCN_TYPE_SBAS	0x10
#define CCN_TYPE_RNI_1P	0x14
#define CCN_TYPE_RNI_2P	0x15
#define CCN_TYPE_RNI_3P	0x16
#define CCN_TYPE_RND_1P	0x18 /* RN-D = RN-I + DVM */
#define CCN_TYPE_RND_2P	0x19
#define CCN_TYPE_RND_3P	0x1a
#define CCN_TYPE_CYCLES	0xff /* Pseudotype */

#define CCN_REGION_SIZE	0x10000

#define CCN_NUM_PMU_EVENT_COUNTERS	8 /* See DT.dbg_id.num_pmucntr */
#define CCN_NUM_PREDEFINED_MASKS	4

#define CCN_DT_PMOVSR			0x0198
#define CCN_DT_PMOVSR_CLR		0x01a0
#define CCN_DT_PMOVSR_CLR__MASK				0x1f

#define CCN_NUM_PMU_EVENT_COUNTERS	8 /* See DT.dbg_id.num_pmucntr */
#define CCN_IDX_PMU_CYCLE_COUNTER	CCN_NUM_PMU_EVENT_COUNTERS

#define CCN_NUM_PMU_EVENTS		4
#define CCN_NUM_XP_WATCHPOINTS		2 /* See DT.dbg_id.num_watchpoints */

struct ccn_ctrl {
	unsigned sbas_present:1;
	unsigned sbsx_present:1;
	struct {
		int num_xps;
		phys_addr_t *base;
	} xp;
	struct {
		int num_nodes;
		phys_addr_t *base;
	} node;
	int num_nodes;
	int mn_id;
	unsigned int irq;
};

struct arm_ccn_component {
	void __iomem *base;
	u32 type;

	DECLARE_BITMAP(pmu_events_mask, CCN_NUM_PMU_EVENTS);
	union {
		struct {
			DECLARE_BITMAP(dt_cmp_mask, CCN_NUM_XP_WATCHPOINTS);
		} xp;
	};
};

struct arm_ccn_dt {
	struct device *dev;
	int id;
	void __iomem *base;

	spinlock_t config_lock;

	DECLARE_BITMAP(pmu_counters_mask, CCN_NUM_PMU_EVENT_COUNTERS + 1);
	struct {
		struct arm_ccn_component *source;
		struct perf_event *event;
	} pmu_counters[CCN_NUM_PMU_EVENT_COUNTERS + 1];

	struct {
	       u64 l, h;
	} cmp_mask[CCN_NUM_PMU_EVENT_COUNTERS + CCN_NUM_PREDEFINED_MASKS];

	struct hrtimer hrtimer;

	cpumask_t cpu;
	struct hlist_node node;

	struct pmu pmu;

	int irq;
};

struct arm_ccn;

#ifdef CONFIG_ARM_CCN
extern bool ccn_probed(void);
#else
static inline int ccn_probed(void) { return false; }
#endif

#ifdef CONFIG_ARM_CCN_PMU
extern struct arm_ccn *arm_ccn_glob;
extern irqreturn_t arm_ccn_pmu_overflow_handler(struct arm_ccn_dt *dt);
#endif

#ifdef CONFIG_ARM_CCN_L3_EDAC
/* to be defined */
#endif

#endif /*__LINUX_ARM_CCN_H*/
