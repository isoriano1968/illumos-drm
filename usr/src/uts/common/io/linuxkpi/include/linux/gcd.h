/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's greatest common divisor (an implementation of the interface,
 * Euclid's algorithm).
 */
#ifndef _LINUX_GCD_H
#define	_LINUX_GCD_H
static inline unsigned long
gcd(unsigned long a, unsigned long b)
{
	while (b != 0) {
		unsigned long t = a % b;
		a = b;
		b = t;
	}
	return (a);
}
#endif
