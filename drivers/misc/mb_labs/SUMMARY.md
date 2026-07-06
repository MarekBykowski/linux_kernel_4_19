# mb_labs — Kernel Labs Interview Guide

Linux 4.19, arm64, ARM FVP-Base. 22 loadable-module labs under
`drivers/misc/mb_labs/`, run by `labs-run.sh`. **Core concepts** first (most
interviewable), then each lab with mechanism + gotcha + likely follow-up.

---

# PART 1 — Core concepts

## 1. The four kernel execution contexts
The kernel distinguishes contexts by bitfields in `current->preempt_count`,
queried with the `in_*()` macros.

| # | Context | Macro | May sleep? | Entered from |
|---|---------|-------|:---:|--------------|
| 1 | **Process/task** | `in_task()` | **yes** | syscall, kthread, workqueue handler, module init/exit |
| 2 | **Softirq** | `in_softirq()` | no | tasklets, timer softirq, net RX/TX, RCU callbacks |
| 3 | **Hardirq** | `in_irq()` | no | device IRQ, timer tick, IPI callback, hrtimer callback |
| 4 | **NMI** | `in_nmi()` | no | pseudo-NMI (arm64), perf, hard-lockup watchdog |

- Contexts 2–4 = **interrupt context** (`in_interrupt()`); **none may sleep** —
  no `msleep`, `mutex_lock`, `kmalloc(GFP_KERNEL)`, `copy_*_user`.
- **5th practical state — process-but-atomic:** a task holding a spinlock, or in
  `preempt_disable()`/`local_bh_disable()`. `in_task()==1` but `in_atomic()==1` →
  **must not sleep** even though you never left process context.
- *"Why can't a hardirq sleep?"* No backing schedulable entity to switch away
  from, and it may have interrupted a lock holder → sleeping risks deadlock and
  there is nothing to schedule back.

## 2. preempt_count — the meter behind all of it
One per-task word, split into fields (`include/linux/preempt.h`):
```
bits  0-7   preempt-disable count   (spin_lock, preempt_disable, get_cpu)
bits  8-15  softirq count           SOFTIRQ_OFFSET = 0x0000_0100
bits 16-19  hardirq count           HARDIRQ_OFFSET = 0x0001_0000
bit  20     NMI                     NMI_OFFSET     = 0x0010_0000
```
`context_lab` prints it live: `0x0` process · `0x1` +spinlock · `0x101` softirq ·
`0x10001` hardirq — each context lights exactly one field.

- `in_atomic()` = `preempt_count != 0` (any cause). Mechanically == "preemption
  disabled now", but its **purpose is "cannot sleep"**. **Caveat:** on
  `CONFIG_PREEMPT=n`, `spin_lock` does not bump the count, so `in_atomic()` can be
  false while holding a lock → it's unreliable; the real sleep gate is
  `might_sleep()` / `CONFIG_DEBUG_ATOMIC_SLEEP`.
- `in_softirq()` = softirq field != 0 → **also true when merely BH-disabled**, not
  only when serving a softirq. The precise test is `in_serving_softirq()`.
  Trick: `local_bh_disable()` adds `2*SOFTIRQ_OFFSET`, serving a softirq adds `1*`
  — the low bit separates "serving" from "disabled".

## 3. `current` on arm64  (very interviewable for arm64 roles)
- `current` is a macro → `get_current()` → **`mrs sp_el0`**, cast to
  `struct task_struct *`.
- The kernel runs with `SPSel=1`, so its stack is `SP_EL1` (or `SP_EL2` under
  VHE). That leaves **SP_EL0 unused as a stack**, so Linux repurposes it to hold
  the `current` pointer. `__switch_to` writes the next task there on every switch
  → `current` always tracks the running task with a single cheap register read.
- `get_current()` uses **non-volatile** asm on purpose so the compiler can
  **cache** `current` (its value is invariant for a running thread). Opposite of
  `READ_ONCE`, which forces re-reads.
- **kthread:** `current` is valid (the kthread's own task), but `current->mm ==
  NULL` (no userspace) — it borrows `active_mm` (lazy TLB). `mm==NULL` does **not**
  mean "not process context"; schedulability comes from having a task, not an mm.
- syscall / module init: `current` **is** the calling user process (`modprobe`),
  with a real `mm` — it runs "on behalf of" that task. A kthread/kworker does not.

## 4. Memory-ordering ladder (compiler vs CPU vs mutual exclusion)
- **`READ_ONCE`/`WRITE_ONCE`** = volatile cast; force exactly one real load/store.
  Stop the *compiler* from caching / tearing / fusing shared accesses.
  **Not atomic. Not ordering.** Minimum for any lockless shared variable.
- **`atomic_t` + `atomic_inc`** = atomic read-modify-write (arm64 `ldxr/stxr`
  exclusives, or LSE `stadd`). Fixes lost updates.
- **`smp_store_release`/`smp_load_acquire`, `rcu_assign_pointer`/`rcu_dereference`**
  = ordering across CPUs ("publish data, then pointer").
- **spinlock / mutex** = mutual exclusion; inside a lock, plain accesses are fine.

## 4b. Kernel linked list (`list_head`) — the most-used structure
```c
struct item { int id; struct list_head node; };   /* node EMBEDDED in the object */
LIST_HEAD(head);                                   /* sentinel; empty = points to self */
list_add_tail(&it->node, &head);                   /* O(1); list_add = prepend */
struct item *it = container_of(ptr, struct item, node);   /* node* -> object* */
list_for_each_entry(it, &head, node) { ... }       /* hides container_of */
list_for_each_entry_safe(it, tmp, &head, node) { list_del(&it->node); kfree(it); }
```
- **Intrusive:** links live *inside* the object (not a separate node holding a
  pointer). → no per-node alloc, and one object can be on **several lists at once**
  (multiple `list_head` members). Opposite of a C++ `std::list`.
- **`container_of(ptr, type, member)`** = `ptr - offsetof(type, member)`; the whole
  trick. Recovers the object from any embedded member. (Same macro `workqueue_lab`
  uses on `work_struct`.)
- **Circular + doubly-linked with a sentinel head:** the ends aren't special-cased
  (no NULL), so `list_add`/`list_del` are branchless. `list_empty()` = head points
  to itself.
- Use **`_safe`** when deleting during iteration (it caches `next` first). RCU
  variants: `list_add_rcu` / `list_for_each_entry_rcu` (pairs with `rcu_lab`).
- Also: `hlist_head` (single-pointer head) for hash tables — half the head size.

## 4c. `container_of` — member pointer → enclosing object
`ptr` is a pointer to the **embedded member**; `container_of` returns a pointer
to the **whole struct** that contains it.
```
container_of(ptr, type, member)
              |     |      |
              |     |      +- name of the field inside the struct  (e.g. "node")
              |     +-------- the struct type                       (e.g. "struct item")
              +-------------- a pointer to that field
```
```c
struct item { int id; struct list_head node; };
struct list_head *ptr = &it->node;                 /* points at the member  */
struct item *obj = container_of(ptr, struct item, node);   /* obj == it     */

#define container_of(ptr, type, member) \
        ((type *)( (char *)(ptr) - offsetof(type, member) ))
```
- **Why:** the member sits at a fixed offset in the struct, so subtract that
  offset from the member's address to reach the struct's start. Live proof from
  list_lab: `node@…508 → item@…500` (node at offset 8: `int id` + padding).
- **Why it exists:** a callback/iterator only receives a pointer to the *embedded
  member* (a `list_head*`, `work_struct*`, `timer_list*`) because the generic
  facility doesn't know your object type. `container_of` walks back to the object.
  `list_for_each_entry` and the workqueue handler both use it internally.
- **Caveat:** pure pointer arithmetic — no type check. Wrong member/type →
  silently bogus address. Names must match the real embedding.

## 5. Bottom-half / deferred-work mechanisms
| Mechanism | Context | Sleep? | Notes |
|-----------|---------|:---:|-------|
| softirq (raw) | softirq | no | 10 fixed vectors, **core subsystems only**, reentrant across CPUs |
| tasklet *(deprecated)* | softirq | no | client of TASKLET softirq; **serialized** (one CPU at a time) |
| timer_list | softirq | no | TIMER softirq, jiffy granularity |
| hrtimer | **hardirq** | no | ns precision (non-RT); red-black tree |
| workqueue | **process** | **yes** | kworker pool; modern default replacement for tasklets |
| threaded IRQ | **process** | **yes** | `request_threaded_irq` → dedicated `irq/<n>` kthread |
| irq_work | hardirq | no | defer from NMI/hardirq to a safe hardirq point |
| BH workqueue (`WQ_BH`) | softirq | no | sanctioned tasklet successor, **kernel 6.9+** (not in 4.19) |

- **A tasklet runs in softirq context but is not a softirq** — it's a callback the
  TASKLET-softirq handler dispatches. A module can't register a softirq vector
  (fixed enum) → it uses a tasklet to reach softirq context.
- Choose by "can it sleep?": yes → workqueue / threaded IRQ; no & low-latency →
  softirq (core) / tasklet (legacy) / `WQ_BH` (6.9+).

### How a softirq/tasklet actually gets serviced
```
your ctx_tasklet_fn()          <- the callback
  ^ called by
tasklet_action()               <- handler for the TASKLET_SOFTIRQ vector
  ^ called by
__do_softirq()                 <- THE dispatcher: loops the pending bitmask,
                                  runs each pending vector's handler
  ^ called from ONE of these three drain points (first to fire wins):
  |-- irq_exit() -> invoke_softirq()     (a hardware IRQ is returning)  [common path]
  |-- local_bh_enable() -> do_softirq()  (someone re-enabled bottom halves)
  +-- run_ksoftirqd()                    (the ksoftirqd/N daemon got scheduled)
```
- The softirq is **not a thread** — `__do_softirq` borrows whatever task is
  `current` at the drain point. So a tasklet is serviced by: the **interrupted
  task** (often `swapper/N`, the common inline path), **`ksoftirqd/N`** (overflow:
  raised in process ctx, or inline budget of ~10 loops/2 ms exceeded), or **another
  kthread** that hit `local_bh_enable` first (seen live: `rcuc/N`).
- `in_serving_softirq()` is true regardless of who runs it; `current->comm` reveals
  which. Per-CPU: raised on CPUn → serviced on CPUn.

---

# PART 2 — The labs

### Address space & memory
- **vma_lab** — kernel vs userspace view of one address space. `kernel_addr.ko`
  walks `mm->mmap` (the VMA list) and prints code/data/stack; `user_addr`
  (userspace) prints its own `/proc/self/maps`-style view. Also shows
  `current == SP_EL0`. *Follow-up:* what's a VMA? → a contiguous region with one
  set of perms/backing in `mm_struct`.
- **mmap_lab** — zero-copy sharing. Char/misc device `.mmap` uses
  `remap_pfn_range()` to map a kernel page into userspace; kernel and user then
  read/write the **same physical page**. `user_mmap` proves the round-trip.
- **ptwalk_lab** — resolve a user VA by hand: `pgd → pud → pmd → pte`, print the
  physical addr and PTE attribute bits (AF/young, UXN/PXN, RO/RW, nG). arm64 4.19
  predates `p4d` (so `pud_offset(pgd,...)`). *Follow-up:* 4-level 4 KB paging,
  39/48-bit VAs; block mappings end the walk early.
- **procfs_lab** — `/proc/my_bool` knob. `proc_create` + `file_operations`
  (`copy_to_user`, `kstrtoint_from_user`); one-shot read via `*ppos`. 4.19 predates
  `proc_ops`.

### Synchronization
- **completion_lab** — one-shot handshake. `wait_for_completion()` blocks init
  until the worker `complete()`s. *Gotcha:* an on-stack completion + a still-running
  worker (e.g. with `_timeout`) = use-after-free; use `DECLARE_COMPLETION_ONSTACK`
  only when the waiter outlives the completer.
- **waitqueue_lab** — sleep until a condition. Consumer `wait_event(wq, cond)`;
  producer sets cond + `wake_up()`. `wait_event` re-checks the condition on every
  wake (handles spurious wakeups).
- **workqueue_lab** — defer to process context. `alloc_workqueue`, `queue_work`;
  the `work_struct` is embedded in an object, recovered with `container_of`.
  Handler runs on a **kworker** and **may sleep**. *Follow-up:* workqueue vs
  tasklet? → process ctx (sleepable) vs softirq (not).
- **locking_lab** — spinlock vs mutex, both give exact counts under contention.
  `sleep_in_atomic=1` sleeps under the spinlock → **`BUG: scheduling while
  atomic`** (and can livelock: a sleeper holding a spinlock spins every other CPU).
  Mutex owners *may* sleep; spinlock holders may not.
- **lockdep_lab** — take locks A→B then B→A in one thread. With
  `CONFIG_PROVE_LOCKING`, lockdep prints **"possible circular locking dependency"**
  on the *possibility* — no actual deadlock needed. Teaches lock ordering (ABBA).
- **atomics_lab** — plain `int++` (load-add-store, races) vs `atomic_inc`
  (`ldxr/stxr`). *On FVP it loses 0 updates* — the functional simulator runs each
  CPU in quanta, so the load/store windows rarely interleave. Lesson: **"my test
  passes" ≠ correct** for concurrency.
- **seqlock_lab** — lockless readers that **retry** instead of block.
  `read_seqbegin` / `read_seqretry`; writer bumps an even/odd sequence around the
  update. Readers never see a torn (half-updated) value. Great for
  rarely-written, often-read data (e.g. timekeeping).
- **rcu_lab** — lockless read + safe reclaim. Reader:
  `rcu_read_lock`/`rcu_dereference`/`rcu_read_unlock`. Writer: `kmalloc` new,
  `rcu_assign_pointer`, `synchronize_rcu` (wait for readers), then free old.
  Readers pay ~nothing; writers pay the grace period.

### Data structures
- **list_lab** — the kernel's **intrusive doubly-linked list** (`list_head`), the
  most-used structure in the kernel. Node embedded in the object; build with
  `list_add_tail`, walk with `list_for_each_entry`, tear down with
  `list_for_each_entry_safe` + `list_del` + `kfree`; `container_of` recovers the
  object from its node. See "Kernel linked list" in Part 1.
- **radix_tree_lab** — sparse index→pointer map. `radix_tree_insert/lookup`,
  `radix_tree_for_each_slot` under `rcu_read_lock`. *Follow-up:* replaced by
  **XArray** in v4.20 (same semantics, nicer API).

### Execution context & CPUs (arm64)
- **context_lab** — the headline lab. Runs one report fn from process, spinlock,
  softirq (tasklet), hardirq (hrtimer); prints `preempt_count` + all `in_*` flags
  so each context shows its bitfield. NMI documented as unreachable from a module
  on 4.19 arm64 (needs pseudo-NMI + `request_nmi`, v5.1+).
- **current_lab** — `current` from init (=`modprobe`, `mm=user`), a kthread
  (its own task, `mm=NULL`), and a hardirq (the interrupted task). `match=1` every
  time proves `current == SP_EL0`.
- **sysreg_lab** — `read_sysreg` of MIDR_EL1 (impl/part), MPIDR_EL1 (affinity),
  CNTVCT/CNTFRQ (virtual counter), CurrentEL. On FVP **CurrentEL = EL2** → kernel
  runs at EL2 via **VHE**.
- **ipi_lab** — interrupt a chosen CPU. `smp_call_function_single(cpu, fn, info,
  wait=1)`; delivered as a **GIC SGI**, callback runs in **hardirq** on the target
  (`in_irq=1`). If sender==target it runs the callback **directly** (process ctx,
  `in_irq=0`) — no IPI. *Follow-up:* `_async` variant is callable from atomic/hardirq
  (non-blocking); sync form needs process context.
- **kprobe_lab** — dynamic tracing. `register_kprobe` on `do_sys_open`; kprobes
  patches a **BRK** into the first instruction; `pre_handler` runs on every hit.
  Counts file opens without recompiling/rebooting.

### Timers & interrupts
- **hrtimer_lab** — hrtimer vs timer_list side by side. hrtimer callback fires in
  **hardirq** (`in_irq=1`), timer_list callback in **softirq** (`in_softirq=1`).
  Periodic hrtimer re-arms with `hrtimer_forward_now`; measures per-expiry jitter
  (µs on FVP). Use hrtimer when the deadline matters; timer_list when "roughly
  later" is fine.
- **irqthread_lab** — threaded IRQ with **no hardware**. `irq_sim` hands out a real
  irq number; `request_threaded_irq` gives a primary top half (hardirq, returns
  `IRQ_WAKE_THREAD`) and a `thread_fn` bottom half (its own `irq/<n>` kthread,
  **sleeps**). Fired from software via `irq_sim_fire`. *Kconfig note:* `IRQ_SIM` is
  promptless → the lab must `select` it (can't be set by a fragment/menuconfig).
- **pacing_lab** — hrtimer packet pacing, the `sch_fq` pattern, no NIC. The hrtimer
  fires in **hardirq** and does **only** `tasklet_schedule()` (it can't transmit —
  that's heavy/sleepable); the tasklet (softirq) "sends" and re-arms the hrtimer
  using **absolute** time so per-packet jitter doesn't accumulate into drift.
  Achieves µs-accurate spacing. *Key point:* hrtimer supplies the precise "when";
  the softirq does the heavy "what" — top-half/bottom-half split.

---

# PART 3 — Rapid-fire

## API pairs to recite
- `smp_call_function_single` (blocks, **process ctx only**) vs `..._single_async`
  (returns immediately, callable from **atomic/hardirq**) vs
  `smp_call_function_many` / `on_each_cpu` (broadcast).
- `msleep` / `usleep_range` (sleep, process) vs `udelay` / `mdelay` (busy-wait,
  any ctx incl. atomic).
- `spin_lock` vs `spin_lock_bh` (also excludes softirqs on this CPU) vs
  `spin_lock_irqsave` (also excludes hardirqs). `_bh`/`_irq` are **local**; the
  spinlock itself covers other CPUs.
- `current_uid()` (own creds, read directly) vs `task_uid(t)` (any task, RCU-safe).
- `get_online_cpus()`/`put_online_cpus()` = CPU-hotplug read lock (freeze the
  online set) vs `get_cpu()`/`put_cpu()` = pin *which* CPU you're on
  (preempt-disable).

## arm64 exception levels & boot
EL0 user · **EL1** kernel (or **EL2 under VHE**, as on FVP) · EL2 hypervisor ·
EL3 secure firmware (TF-A). syscall = `svc` (EL0→kernel); firmware/PSCI = `smc`
(→EL3). **Boot chain:** BL1 → BL2 → BL31 (TF-A) → U-Boot → Linux.

## Yocto
- Kernel config **fragment** = a `*.cfg` file in `SRC_URI`; `kernel-yocto`
  auto-merges it (extension `.cfg` is the trigger).
- **`select` vs `depends on`:** `select X` forces X on (ignores X's own deps —
  can create invalid configs); `depends on X` means *you* must enable X (and its
  chain) first. A **promptless** symbol (no menu entry, e.g. `IRQ_SIM`) **cannot**
  be set by a fragment or `menuconfig` — it must be `select`ed by another symbol.
- Reliable config workflow: `bitbake -c menuconfig virtual/kernel`, then
  `bitbake -c diffconfig virtual/kernel` → auto-generates the fragment (deps
  already resolved).
- QA **`buildpaths`**: a packaged file embeds `$TMPDIR` (absolute build path) →
  breaks reproducibility / leaks host info. Fix with `-fdebug-prefix-map`, or a
  narrow `INSANE_SKIP:${PN}-src += "buildpaths"` for harmless debug-source cases.

## One-liners that impress
- "On arm64 `current` is just `mrs sp_el0` — SP_EL0 is free because the kernel
  runs on SP_EL1/EL2."
- "`preempt_count` is the single word that answers 'can I sleep here'."
- "A tasklet runs in softirq context but isn't a softirq."
- "hrtimer = precise *when* (hardirq); the softirq/workqueue does the *what*."
- "lockdep flags the *possibility* of deadlock, not the occurrence."
