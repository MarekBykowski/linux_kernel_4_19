# Final Glance — 5 min before the interview

**Contexts (4)** — told apart by `current->preempt_count`, checked with `in_*()`:
`process` (sleeps ✓) · `softirq` · `hardirq` · `nmi`. 2–4 = `in_interrupt()`, no sleep.
5th state: **process-but-atomic** (spinlock / preempt_disable): `in_task()=1` but
`in_atomic()=1` → **can't sleep**. Why msleep-under-spinlock = `BUG: scheduling while atomic`.

**preempt_count** (one word): `0x1` preempt/spinlock · `0x100` softirq · `0x10000` hardirq.
`in_atomic()` = count≠0 = "can't sleep" (unreliable on `PREEMPT=n`: spinlock doesn't bump it).
`in_softirq()` also true when just BH-disabled; precise = `in_serving_softirq()`.

**current (arm64)** = `mrs sp_el0`. Kernel runs on SP_EL1/EL2 (SPSel=1) → SP_EL0 free →
holds `current`; `__switch_to` updates it. kthread: `current` valid but `mm==NULL`
(borrows active_mm). Non-volatile asm → cacheable (invariant per thread).

**Ordering ladder:** plain (broken) → `READ_ONCE/WRITE_ONCE` (compiler: 1 access, no
tear/fuse; NOT atomic/ordering) → `atomic_t` (`ldxr/stxr`) → release/acquire, RCU
(ordering) → spinlock/mutex (mutual exclusion).

**Bottom halves:** softirq (10 fixed, core-only) · **tasklet** (runs *in* softirq, isn't
one; deprecated) · timer_list (softirq) · **hrtimer (hardirq)** · **workqueue (process,
sleeps)** · **threaded IRQ (process kthread)** · irq_work (hardirq) · WQ_BH (softirq, 6.9+).

**list_head** = intrusive (node embedded in object → no alloc, object on many lists).
`container_of(ptr,type,member)` = `(char*)ptr - offsetof(type,member)`: member* → object*.
`list_for_each_entry` hides it; use `_safe` to delete while iterating.
`offsetof` = standard C89 (`<stddef.h>`); kernel uses `__builtin_offsetof`.

**API pairs:** `smp_call_function_single` (blocks, process) vs `_async` (atomic/hardirq).
`msleep/usleep_range` (sleep) vs `udelay` (busy, any ctx). `spin_lock_bh` (excl. softirq)
vs `_irqsave` (excl. hardirq) — local; lock covers other CPUs.

**arm64 EL:** EL0 user · EL1 kernel (**EL2/VHE on FVP**) · EL2 hyp · EL3 firmware (TF-A).
`svc`=syscall, `smc`=firmware. Boot: BL1→BL2→BL31→U-Boot→Linux.

**Yocto:** `.cfg` fragment in SRC_URI (auto-merged). `select` (forces on) vs `depends on`
(you enable chain). **Promptless symbol (IRQ_SIM) can't be set by fragment/menuconfig →
must be select'd.** Reliable: `bitbake -c menuconfig` then `-c diffconfig`.

---
**Lead story:** top-half/bottom-half arc — `context_lab` (name contexts) → `hrtimer_lab`
(hardirq vs softirq) → `pacing_lab` (sch_fq: hardirq timer only kicks a softirq) →
`irqthread_lab` (threaded IRQ: primary hardirq `IRQ_WAKE_THREAD` → thread_fn kthread sleeps).

**Killer one-liners:**
- "On arm64 `current` is just `mrs sp_el0` — SP_EL0 is free because the kernel runs on SP_EL1/EL2."
- "`preempt_count` is the one word that answers 'can I sleep here'."
- "A tasklet runs in softirq context but isn't a softirq."
- "hrtimer = precise *when* (hardirq); softirq/workqueue does the *what*."
- "lockdep flags the *possibility* of deadlock, not the occurrence."
- "container_of = member pointer minus its offset → the enclosing object."

**Gotchas:** atomics_lab loses 0 on FVP (quantum sim ≠ real HW — "passes" ≠ correct).
completion/waitqueue: on-stack completion + running worker = UAF. kthread has a kernel
stack (SP_EL1) but no user stack (mm=NULL). radix_tree → XArray in v4.20.
