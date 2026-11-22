/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Memory ordering between two CPUs: store buffering (what a full barrier
 * forbids), message passing (what wmb/rmb and release/acquire forbid),
 * lost wake-ups, kfifo without a lock, and the helpers with code of their
 * own on aarch64.
 *
 * Needs two CPUs or more. The "control" runs have no barrier and may see reorderings; they are
 * only reported, to show whether this machine can show a fault at all.
 */

#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/math64.h>
#include <linux/mm.h>
#include <linux/nospec.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "st.h"

#define	ORD_ROUNDS	300000
#define	ORD_MP_ROUNDS	2000000
#define	ORD_WAKE_ROUNDS	100000
#define	ORD_FIFO_ITEMS	2000000
#define	ORD_GIVE_UP	(20 * HZ)	/* a run that gets stuck */

enum ord_op {
	SB_NONE, SB_MB, SB_INC_RETURN, SB_XCHG, SB_CMPXCHG, SB_FETCH_ADD,
	SB_TAS_BIT, SB_SET_STATE,
	MP_NONE, MP_WMB, MP_RELEASE
};

static struct {
	int x;
	char pad0[252];
	int y;
	char pad1[252];
	atomic_long_t meet;
	char pad2[248];
	atomic_t scratch[2];
	int plain[2];
	unsigned long bits[2];
	int r[2];
	char pad3[256];
	unsigned long bad;
	unsigned long done;
	volatile int abort;
	enum ord_op op;
	struct task_struct *waiter;
	long seq;
	char pad4[256];
	long ack;
	long late;
} ord;

static DECLARE_KFIFO(ord_fifo, u32, 64);

static bool
ord_spin(unsigned long *n, unsigned long end)
{
	cpu_relax();
	if ((++*n & 4095) != 0)
		return (true);
	cond_resched();
	if (ord.abort || time_after(jiffies, end)) {
		ord.abort = 1;
		return (false);
	}
	return (true);
}

/* Both threads pass the nth meeting point at the same time. */
static bool
ord_meet(long nth, unsigned long end)
{
	unsigned long n = 0;

	(void) atomic_long_inc_return(&ord.meet);
	while (atomic_long_read(&ord.meet) < 2 * nth) {
		if (!ord_spin(&n, end))
			return (false);
	}
	smp_mb();
	return (true);
}

static void
ord_sb(int me)
{
	unsigned long end = jiffies + ORD_GIVE_UP;
	int *mine = me == 0 ? &ord.x : &ord.y;
	int *other = me == 0 ? &ord.y : &ord.x;
	atomic_t *a = &ord.scratch[me];
	long r, meet = 0;
	int i;

	for (r = 0; r < ORD_ROUNDS; r++) {
		WRITE_ONCE(*mine, 0);
		if (!ord_meet(++meet, end))
			return;
		for (i = (int)((r * (me + 3)) & 7); i > 0; i--)
			barrier();
		WRITE_ONCE(*mine, 1);
		switch (ord.op) {
		case SB_MB:		smp_mb(); break;
		case SB_INC_RETURN:	(void) atomic_inc_return(a); break;
		case SB_XCHG:		(void) xchg(&a->counter, 1); break;
		case SB_CMPXCHG:	(void) cmpxchg(&ord.plain[me], 0, 0); break;
		case SB_FETCH_ADD:	(void) atomic_fetch_add(1, a); break;
		case SB_TAS_BIT:	(void) test_and_set_bit(1, &ord.bits[me]); break;
		case SB_SET_STATE:	set_current_state(TASK_RUNNING); break;
		default:		barrier(); break;
		}
		ord.r[me] = READ_ONCE(*other);
		if (!ord_meet(++meet, end))
			return;
		if (me == 0) {
			if (ord.r[0] == 0 && ord.r[1] == 0)
				ord.bad++;
			ord.done++;
		}
		if (!ord_meet(++meet, end))
			return;
	}
}

static void
ord_mp(int me)
{
	unsigned long end = jiffies + ORD_GIVE_UP, n = 0;
	int i, flag, data;

	if (!ord_meet(1, end))
		return;
	if (me == 0) {
		for (i = 1; i <= ORD_MP_ROUNDS; i++) {
			WRITE_ONCE(ord.x, i);
			if (ord.op == MP_WMB)
				smp_wmb();
			if (ord.op == MP_RELEASE)
				smp_store_release(&ord.y, i);
			else
				WRITE_ONCE(ord.y, i);
			if ((i & 65535) == 0)
				cond_resched();
		}
		return;
	}
	for (;;) {
		if (ord.op == MP_RELEASE)
			flag = smp_load_acquire(&ord.y);
		else
			flag = READ_ONCE(ord.y);
		if (ord.op == MP_WMB)
			smp_rmb();
		data = READ_ONCE(ord.x);
		if (data < flag)
			ord.bad++;
		ord.done++;
		if (flag >= ORD_MP_ROUNDS)
			return;
		if ((++n & 4095) == 0) {
			cond_resched();
			if (time_after(jiffies, end)) {
				ord.abort = 1;
				return;
			}
		}
	}
}

static void
ord_wake(int me)
{
	unsigned long end = jiffies + 3 * ORD_GIVE_UP, n = 0;
	long i, left;

	if (me == 0)
		ord.waiter = current;
	if (!ord_meet(1, end))
		return;
	for (i = 1; i <= ORD_WAKE_ROUNDS; i++) {
		if (me == 0) {
			for (;;) {
				set_current_state(TASK_UNINTERRUPTIBLE);
				if (READ_ONCE(ord.seq) >= i)
					break;
				left = schedule_timeout(HZ / 2);
				/* the condition holds, but nobody woke us */
				if (left == 0 && READ_ONCE(ord.seq) >= i)
					ord.late++;
				if (ord.abort || time_after(jiffies, end)) {
					ord.abort = 1;
					break;
				}
			}
			__set_current_state(TASK_RUNNING);
			if (ord.abort)
				return;
			ord.done++;
			smp_store_release(&ord.ack, i);
		} else {
			WRITE_ONCE(ord.seq, i);
			(void) wake_up_process(ord.waiter);
			while (smp_load_acquire(&ord.ack) < i) {
				if (!ord_spin(&n, end))
					return;
			}
		}
	}
}

static void
ord_kfifo(int me)
{
	unsigned long end = jiffies + ORD_GIVE_UP, n = 0;
	u32 i, v;

	if (!ord_meet(1, end))
		return;
	for (i = 1; i <= ORD_FIFO_ITEMS; i++) {
		if (me == 0) {
			while (!kfifo_put(&ord_fifo, i)) {
				if (!ord_spin(&n, end))
					return;
			}
		} else {
			while (!kfifo_get(&ord_fifo, &v)) {
				if (!ord_spin(&n, end))
					return;
			}
			if (v != i)
				ord.bad++;
			ord.done++;
		}
	}
}

static void (*ord_body)(int);
static struct completion ord_done[2];

static int
ord_thread(void *arg)
{
	int me = (int)(uintptr_t)arg;

	ord_body(me);
	complete(&ord_done[me]);
	while (!kthread_should_stop())
		msleep(1);
	return (0);
}

/* Runs body on two threads; false if it didn't finish. */
static bool
ord_run(void (*body)(int), enum ord_op op)
{
	struct task_struct *t[2] = { NULL, NULL };
	int i;

	memset(&ord, 0, sizeof (ord));
	INIT_KFIFO(ord_fifo);
	ord.op = op;
	ord_body = body;
	smp_mb();
	for (i = 0; i < 2; i++) {
		init_completion(&ord_done[i]);
		t[i] = kthread_run(ord_thread, (void *)(uintptr_t)i, "st_order%d", i);
		if (IS_ERR(t[i])) {
			t[i] = NULL;
			ord.abort = 1;
		}
	}
	for (i = 0; i < 2; i++) {
		if (t[i] != NULL)
			wait_for_completion(&ord_done[i]);
	}
	for (i = 0; i < 2; i++) {
		if (t[i] != NULL)
			(void) kthread_stop(t[i]);
	}
	return (!ord.abort);
}

static void
ord_math(void)
{
	static const struct { u64 a, m, d, q; } v[] = {
		{ 1000, 3, 7, 428 },
		{ ~0ULL, ~0ULL, ~0ULL, ~0ULL },
		{ 1ULL << 63, 10, 20, 1ULL << 62 },
		{ 0x123456789abcdef0ULL, 0x10, 0x100, 0x0123456789abcdefULL },
		{ ~0ULL, 1000000000ULL, 1000000000ULL, ~0ULL },
		{ ~0ULL, 2, 1, ~0ULL },		/* saturates */
		{ 0x8000000000000001ULL, 0xfffffffffffffffeULL,
		    0xffffffffffffffffULL, 0x8000000000000000ULL },
	};
	unsigned int i;
	bool ok = true;

	for (i = 0; i < ARRAY_SIZE(v); i++)
		ok &= mul_u64_u64_div_u64(v[i].a, v[i].m, v[i].d) == v[i].q;
	CHECK("order: mul_u64_u64_div_u64", ok);

	ok = true;
	for (i = 0; i < 20; i++) {
		unsigned long idx = i;

		OPTIMIZER_HIDE_VAR(idx);
		ok &= array_index_nospec(idx, 10UL) == (i < 10 ? i : 0);
	}
	OPTIMIZER_HIDE_VAR(i);
	ok &= array_index_mask_nospec(~0UL, i) == 0;
	ok &= array_index_mask_nospec(0, 0) == 0;
	CHECK("order: array_index_nospec", ok);
}

/*
 * memcpy_toio() and friends at every alignment on a Device mapping of RAM
 * (it faults on an unaligned access), read back through the cached
 * mapping once the uncached one is gone.
 */
static void
ord_io(void)
{
	struct page *p = alloc_page(GFP_KERNEL | __GFP_ZERO);
	u8 src[67], dst[67], *kva, *uc;
	bool same = true, after = true;
	unsigned int to, from, len, i;

	if (p == NULL) {
		CHECK("order: page for the io test", false);
		return;
	}
	kva = page_address(p);
	uc = vmap(&p, 1, VM_MAP, pgprot_noncached(PAGE_KERNEL));
	if (uc == NULL) {
		CHECK("order: uncached vmap", false);
		__free_page(p);
		return;
	}
	for (i = 0; i < sizeof (src); i++)
		src[i] = (u8)(i * 7 + 1);
	for (to = 0; to < 8; to++) {
		for (from = 0; from < 8; from++) {
			for (len = 0; len < 59; len++) {
				memset_io(uc + to, 0, len);
				memcpy_toio(uc + to, src + from, len);
				memcpy_fromio(dst + from, uc + to, len);
				same &= bcmp(dst + from, src + from, len) == 0;
			}
		}
	}
	CHECK("order: io copies at every alignment", same);

	/* fill the cached lines, then write behind their back */
	for (i = 0; i < PAGE_SIZE; i++)
		dst[i % sizeof (dst)] ^= READ_ONCE(kva[i]);
	memset_io(uc, 0xa5, PAGE_SIZE);
	writel(0x12345678, uc + 16);
	CHECK("order: writel/readl", readl(uc + 16) == 0x12345678);
	vunmap(uc);
	for (i = 0; i < PAGE_SIZE; i++) {
		if (i < 16 || i >= 20)
			after &= READ_ONCE(kva[i]) == 0xa5;
	}
	after &= READ_ONCE(*(u32 *)(void *)(kva + 16)) == 0x12345678;
	CHECK("order: cached view current after vunmap of an uncached one",
	    after);
	__free_page(p);
}

void
st_order_tests(void)
{
	static const struct { enum ord_op op; const char *name; } sb[] = {
		{ SB_MB, "order: SB with smp_mb" },
		{ SB_INC_RETURN, "order: SB with atomic_inc_return" },
		{ SB_XCHG, "order: SB with xchg" },
		{ SB_CMPXCHG, "order: SB with cmpxchg" },
		{ SB_FETCH_ADD, "order: SB with atomic_fetch_add" },
		{ SB_TAS_BIT, "order: SB with test_and_set_bit" },
		{ SB_SET_STATE, "order: SB with set_current_state" },
	};
	unsigned int i;
	bool ok;

	ord_math();
	ord_io();

	ok = ord_run(ord_sb, SB_NONE);
	pr_info("linuxkpi_selftest: control SB: %lu reorderings in %lu rounds%s\n",
	    ord.bad, ord.done, ok ? "" : " (unfinished)");
	for (i = 0; i < ARRAY_SIZE(sb); i++) {
		ok = ord_run(ord_sb, sb[i].op);
		pr_info("linuxkpi_selftest: %s: %lu of %lu\n", sb[i].name,
		    ord.bad, ord.done);
		CHECK(sb[i].name, ok && ord.bad == 0 && ord.done == ORD_ROUNDS);
	}

	ok = ord_run(ord_mp, MP_NONE);
	pr_info("linuxkpi_selftest: control MP: %lu reorderings in %lu reads%s\n",
	    ord.bad, ord.done, ok ? "" : " (unfinished)");
	ok = ord_run(ord_mp, MP_WMB);
	CHECK("order: MP with smp_wmb/smp_rmb", ok && ord.bad == 0);
	ok = ord_run(ord_mp, MP_RELEASE);
	CHECK("order: MP with release/acquire", ok && ord.bad == 0);

	ok = ord_run(ord_wake, SB_NONE);
	pr_info("linuxkpi_selftest: wake-ups: %ld lost of %lu\n", ord.late,
	    ord.done);
	CHECK("order: no lost wake-up", ok && ord.late == 0 &&
	    ord.done == ORD_WAKE_ROUNDS);

	ok = ord_run(ord_kfifo, SB_NONE);
	CHECK("order: kfifo, one producer and one consumer",
	    ok && ord.bad == 0 && ord.done == ORD_FIFO_ITEMS);
}
