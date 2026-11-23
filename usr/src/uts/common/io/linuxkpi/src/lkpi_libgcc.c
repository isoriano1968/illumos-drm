/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * The libgcc helpers gcc calls for __builtin_clzll/__builtin_ctzll where
 * the CPU has no such instruction (sparcv9 before VIS3: UltraSPARC
 * III/IIIi). The kernel links no libgcc, so the layer carries them.
 */
#include <linux/types.h>

#if defined(__sparc)

int __clzdi2(unsigned long long);
int __ctzdi2(unsigned long long);
unsigned int __bswapsi2(unsigned int);

unsigned long long __bswapdi2(unsigned long long);

/* For __builtin_bswap32/64 in carried code (no bswap instruction). */
unsigned int
__bswapsi2(unsigned int x)
{
	return (((x & 0xffU) << 24) | ((x & 0xff00U) << 8) |
	    ((x >> 8) & 0xff00U) | (x >> 24));
}

unsigned long long
__bswapdi2(unsigned long long x)
{
	return (((unsigned long long)__bswapsi2((unsigned int)x) << 32) |
	    __bswapsi2((unsigned int)(x >> 32)));
}

int
__clzdi2(unsigned long long x)
{
	int n = 0;

	if ((x >> 32) == 0) { n += 32; x <<= 32; }
	if ((x >> 48) == 0) { n += 16; x <<= 16; }
	if ((x >> 56) == 0) { n += 8; x <<= 8; }
	if ((x >> 60) == 0) { n += 4; x <<= 4; }
	if ((x >> 62) == 0) { n += 2; x <<= 2; }
	if ((x >> 63) == 0) n += 1;
	return (n);
}

int
__ctzdi2(unsigned long long x)
{
	int n = 0;

	if ((x & 0xffffffffULL) == 0) { n += 32; x >>= 32; }
	if ((x & 0xffffULL) == 0) { n += 16; x >>= 16; }
	if ((x & 0xffULL) == 0) { n += 8; x >>= 8; }
	if ((x & 0xfULL) == 0) { n += 4; x >>= 4; }
	if ((x & 0x3ULL) == 0) { n += 2; x >>= 2; }
	if ((x & 0x1ULL) == 0) n += 1;
	return (n);
}

#endif	/* __sparc */
