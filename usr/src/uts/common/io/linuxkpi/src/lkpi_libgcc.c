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
__int128 __multi3(__int128, __int128);

/* 128-bit multiply (mul_u64_u64_shr() and friends): the low 128 bits. */
__int128
__multi3(__int128 a, __int128 b)
{
	unsigned long long al = (unsigned long long)a;
	unsigned long long ah = (unsigned long long)(a >> 64);
	unsigned long long bl = (unsigned long long)b;
	unsigned long long bh = (unsigned long long)(b >> 64);
	unsigned long long a0 = al & 0xffffffffULL, a1 = al >> 32;
	unsigned long long b0 = bl & 0xffffffffULL, b1 = bl >> 32;
	unsigned long long p00 = a0 * b0, p01 = a0 * b1;
	unsigned long long p10 = a1 * b0, p11 = a1 * b1;
	unsigned long long mid = (p00 >> 32) + (p01 & 0xffffffffULL) +
	    (p10 & 0xffffffffULL);
	unsigned long long lo = (p00 & 0xffffffffULL) | (mid << 32);
	unsigned long long hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32) +
	    al * bh + ah * bl;

	return ((__int128)(((unsigned __int128)hi << 64) | lo));
}
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
