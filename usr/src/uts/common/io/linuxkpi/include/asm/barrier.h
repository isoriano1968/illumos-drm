/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's memory barriers (Documentation/memory-barriers.txt, and
 * arch/x86/include/asm/barrier.h, GPL-2.0; reimplemented).
 *
 * x86 is TSO: the smp_ read/write barriers are compiler barriers; full
 * barriers are mfence (illumos membar_enter() is a locked instruction
 * with the same effect).
 *
 * aarch64 is weakly ordered: each barrier is an instruction, from the Arm
 * ARM's definition of it. dsb for mb/rmb/wmb (which also order against
 * devices and wait for completion), dmb osh for the dma_ forms, dmb ish
 * between CPUs. __lkpi_atomic_fence() goes before and after each atomic
 * operation that Linux defines as fully ordered: a seq_cst operation
 * (ldaxr/stlxr) lets a later load pass its store, and an earlier store
 * pass its load, so by itself it is no full barrier.
 */
#ifndef _ASM_BARRIER_H
#define	_ASM_BARRIER_H

#include <linux/compiler.h>

#if defined(__sparc)

/*
 * sparcv9, TSO (the memory model illumos runs sun4u in): loads are not
 * reordered with loads, stores not with stores, only a later load may pass
 * an earlier store.  membar #Sync also waits for outstanding device and
 * DMA accesses, as mb() and friends must.
 */
#define	__lkpi_barrier(op)	__asm__ __volatile__(op ::: "memory")
#define	mb()		__lkpi_barrier("membar #Sync")
#define	rmb()		__lkpi_barrier("membar #Sync")
#define	wmb()		__lkpi_barrier("membar #Sync")
#define	dma_mb()	__lkpi_barrier("membar #Sync")
#define	dma_rmb()	__lkpi_barrier("membar #LoadLoad")
#define	dma_wmb()	__lkpi_barrier("membar #StoreStore")
#define	smp_mb()	__lkpi_barrier("membar #StoreLoad")
#define	smp_rmb()	__lkpi_barrier("")
#define	smp_wmb()	__lkpi_barrier("")
#define	smp_store_release(p, v)	__atomic_store_n((p), (v), __ATOMIC_RELEASE)
#define	smp_load_acquire(p)	__atomic_load_n((p), __ATOMIC_ACQUIRE)
#define	smp_mb__before_atomic()	smp_mb()
#define	smp_mb__after_atomic()	smp_mb()
/*
 * Fully ordered atomics: a casx is both a load and a store, and under TSO
 * a later load may still pass its store half, so a #StoreLoad goes around.
 */
#define	__lkpi_atomic_fence()	smp_mb()
#define	__lkpi_acquire_after_load()	barrier()
/* UltraSPARC III/IIIi has no pause instruction; illumos spins on a nop. */
#define	__LKPI_CPU_RELAX	"nop"

#elif defined(__aarch64__)

#define	__lkpi_barrier(op)	__asm__ __volatile__(op ::: "memory")
#define	mb()		__lkpi_barrier("dsb sy")
#define	rmb()		__lkpi_barrier("dsb ld")
#define	wmb()		__lkpi_barrier("dsb st")
#define	dma_mb()	__lkpi_barrier("dmb osh")
#define	dma_rmb()	__lkpi_barrier("dmb oshld")
#define	dma_wmb()	__lkpi_barrier("dmb oshst")
#define	smp_mb()	__lkpi_barrier("dmb ish")
#define	smp_rmb()	__lkpi_barrier("dmb ishld")
#define	smp_wmb()	__lkpi_barrier("dmb ishst")
#define	smp_store_release(p, v)	__atomic_store_n((p), (v), __ATOMIC_RELEASE)
#define	smp_load_acquire(p)	__atomic_load_n((p), __ATOMIC_ACQUIRE)
#define	smp_mb__before_atomic()	smp_mb()
#define	smp_mb__after_atomic()	smp_mb()
#define	__lkpi_atomic_fence()	smp_mb()
#define	__lkpi_acquire_after_load()	smp_rmb()
#define	__LKPI_CPU_RELAX	"yield"

#elif defined(__x86_64__)

#define	mb()		__asm__ __volatile__("mfence" ::: "memory")
#define	rmb()		__asm__ __volatile__("lfence" ::: "memory")
#define	wmb()		__asm__ __volatile__("sfence" ::: "memory")
#define	dma_rmb()	barrier()
#define	dma_wmb()	barrier()
#define	dma_mb()	mb()
#define	smp_mb()	__asm__ __volatile__("lock; addl $0,-4(%%rsp)" ::: "memory", "cc")
#define	smp_rmb()	barrier()
#define	smp_wmb()	barrier()
#define	smp_store_release(p, v)	do { barrier(); WRITE_ONCE(*(p), (v)); } while (0)
#define	smp_load_acquire(p)	({ typeof (*(p)) __v = READ_ONCE(*(p)); barrier(); __v; })
#define	smp_mb__before_atomic()	barrier()
#define	smp_mb__after_atomic()	barrier()
#define	__lkpi_atomic_fence()	((void)0)
#define	__lkpi_acquire_after_load()	barrier()
#define	__LKPI_CPU_RELAX	"pause"

#else
#error "no memory barriers for this architecture"
#endif

#define	__smp_mb()	smp_mb()
#define	smp_store_mb(var, value)	do { WRITE_ONCE(var, value); smp_mb(); } while (0)
#define	smp_acquire__after_ctrl_dep()	smp_rmb()
#define	smp_mb__after_spinlock()	smp_mb()
#define	smp_mb__after_unlock_lock()	smp_mb()
#define	virt_mb()	smp_mb()
#define	virt_rmb()	smp_rmb()
#define	virt_wmb()	smp_wmb()
#define	smp_cond_load_relaxed(ptr, cond_expr) ({ \
	typeof (ptr) __PTR = (ptr); \
	typeof (*(ptr)) VAL; \
	for (;;) { \
		VAL = READ_ONCE(*__PTR); \
		if (cond_expr) \
			break; \
		SMT_PAUSE(); \
	} \
	VAL; })
#define	smp_cond_load_acquire(ptr, cond_expr) ({ \
	typeof (*(ptr)) __v = smp_cond_load_relaxed(ptr, cond_expr); \
	__lkpi_acquire_after_load(); __v; })
static inline void
cpu_relax(void)
{
	__asm__ __volatile__(__LKPI_CPU_RELAX ::: "memory");
}
#ifndef	SMT_PAUSE
#define	SMT_PAUSE()	cpu_relax()
#endif

#endif
