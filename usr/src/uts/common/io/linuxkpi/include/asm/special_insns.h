/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's x86 cache-line flush instructions: clflush, and clflushopt
 * (66 0F AE /7) where the CPU has it, else clflush.
 */
#ifndef _ASM_SPECIAL_INSNS_H
#define	_ASM_SPECIAL_INSNS_H

#include <asm/cpufeature.h>

#if defined(__sparc)
/*
 * sun4u: the caches are coherent with DMA (the IOMMU and the Schizo/PCIe
 * bridges keep them so), so there is nothing to write back; order only.
 */
static inline void
clflush(volatile void *p)
{
	__asm__ __volatile__("" : : "r" (p) : "memory");
}

#define	clflushopt(p)	clflush(p)

static inline unsigned long
lkpi_dcache_line(void)
{
	return (64);	/* UltraSPARC III/IIIi E-cache line */
}

static inline void
wbinvd(void)
{
	__asm__ __volatile__("membar #Sync" ::: "memory");
}

#elif defined(__aarch64__)
/*
 * The same operation on aarch64: clean and invalidate one line to the
 * point of coherency. There is no wbinvd.
 */
static inline void
clflush(volatile void *p)
{
	__asm__ __volatile__("dc civac, %0" : : "r" (p) : "memory");
}

#define	clflushopt(p)	clflush(p)

static inline unsigned long
lkpi_dcache_line(void)
{
	unsigned long ctr;

	/* CTR_EL0.DminLine: log2 of the smallest line, in words */
	__asm__ ("mrs %0, ctr_el0" : "=r" (ctr));
	return (4UL << ((ctr >> 16) & 0xf));
}

#else	/* !__aarch64__ */

static inline void
clflush(volatile void *p)
{
	__asm__ __volatile__("clflush %0" : "+m" (*(volatile char *)p));
}

static inline void
clflushopt(volatile void *p)
{
	if (boot_cpu_has(X86_FEATURE_CLFLUSHOPT))
		__asm__ __volatile__(".byte 0x66; clflush %0"
		    : "+m" (*(volatile char *)p));
	else
		clflush(p);
}

static inline void
wbinvd(void)
{
	__asm__ __volatile__("wbinvd" ::: "memory");
}

#endif	/* __sparc, __aarch64__ */

#endif
