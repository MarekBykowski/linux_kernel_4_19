# Kernel execution contexts

The Linux kernel runs code in **four** execution contexts. It tells them
apart with the bitfields packed into a single per-task word,
`current->preempt_count`, read through the `in_*()` macros.

| # | Context | Macro | May sleep? | Typical sources |
|---|---------|-------|:----------:|-----------------|
| 1 | **Process (task)** | `in_task()` | **yes** | syscall path, kthread, workqueue handler, module init/exit |
| 2 | **Softirq** (bottom half) | `in_softirq()` | no | network RX/TX, tasklets, timer softirq, RCU callbacks |
| 3 | **Hardirq** (top half) | `in_irq()` | no | device IRQ handler, timer tick, IPI callback |
| 4 | **NMI** | `in_nmi()` | no | pseudo-NMI (arm64), perf, hard-lockup watchdog |

Contexts 2–4 are collectively **interrupt context** (`in_interrupt()`), and
**none of them may sleep** — no `msleep`, no `mutex_lock`, no
`smp_call_function_single`. Only process context can block.

## The nuance: process context has two sub-states

The question that actually matters — *"can I sleep here?"* — has a **fifth
answer** that isn't one of the four contexts:

- **preemptible** (normal process context): can sleep, take mutexes, call
  the blocking primitives.
- **atomic** (`in_atomic()` true, but `in_interrupt()` false): still a
  task, but preemption is disabled or a `spin_lock` is held — **must not
  sleep**, even though you never left process context.

This is why `locking_lab sleep_in_atomic=1` splats: the code runs in a
kthread (process context) but under a spinlock, so `preempt_count` is
nonzero → `might_sleep()` / `CONFIG_DEBUG_ATOMIC_SLEEP` catches the
`msleep()` with *"BUG: sleeping function called from invalid context"*.

So sleeping depends on **`preempt_count` as a whole**, not just on which of
the four contexts you are in.

## The meter itself: `preempt_count`

One word, split into bitfields (offsets from `include/linux/preempt.h`):

```
 bits  0– 7   preempt-disable count   (spin_lock, preempt_disable)
 bits  8–15   softirq count           SOFTIRQ_OFFSET  = 0x0000_0100
 bits 16–19   hardirq count           HARDIRQ_OFFSET  = 0x0001_0000
 bit  20      NMI                      NMI_OFFSET      = 0x0010_0000
```

`in_atomic()` is simply "any of these nonzero". `in_irq=65536` you saw in
the ipi lab is `0x10000` — the hardirq bit set — versus `in_irq=0` when the
callback happened to run directly in process context (sender == target).

## Seen live — `context_lab`

`context_lab` runs one reporting function from each reachable context and
prints the meter, so the bitfields are visible per context:

```
context_lab: process   pc=00000000  in_task=1 in_softirq=0 in_irq=0 in_nmi=0  in_atomic=0  cpu=6
context_lab: proc+spin pc=00000001  in_task=1 in_softirq=0 in_irq=0 in_nmi=0  in_atomic=1  cpu=6
context_lab: softirq   pc=00000101  in_task=0 in_softirq=1 in_irq=0 in_nmi=0  in_atomic=1  cpu=6
context_lab: hardirq   pc=00010001  in_task=0 in_softirq=0 in_irq=1 in_nmi=0  in_atomic=1  cpu=6
```

Read `pc=` by nibble:

- `00000000` — plain process context, fully preemptible.
- `00000001` — process context **+ spinlock**: low byte = 1 (one level of
  preempt-disable). `in_task` still 1, but `in_atomic` flips to 1 — the
  "can't sleep here" trap.
- `00000101` — tasklet: `+SOFTIRQ_OFFSET`.
- `00010001` — hrtimer callback: `+HARDIRQ_OFFSET` (the low `01` is the
  hardirq path's own preempt-disable).

### Why no NMI row

NMI is the fourth context but is **not reachable from a module** on this
4.19 arm64 kernel: arm64 only has NMIs with pseudo-NMI support, and the
API to run code there (`request_nmi()`) arrived in v5.1. `report_context()`
would print `in_nmi=1` if it ever ran there.

## Rule of thumb

> **Sleepable primitives** (`msleep`, `mutex_lock`, `smp_call_function_single`,
> `get_online_cpus`) require **process context with preemption on**.
> In any atomic or interrupt context, reach for the non-sleeping variants
> instead (`udelay`, `spin_lock`, `smp_call_function_single_async`,
> `irq_work`).

## Related labs

- `context_lab` — this one; reports the contexts.
- `locking_lab` (`sleep_in_atomic=1`) — sleeping under a spinlock; the
  `DEBUG_ATOMIC_SLEEP` splat.
- `ipi_lab` — `in_irq=65536` (hardirq) vs `in_irq=0` (self-call in process
  context).
- `hrtimer_lab` — timer callback in hardirq context.
- `workqueue_lab` — deferred work back in process context (a kworker).
