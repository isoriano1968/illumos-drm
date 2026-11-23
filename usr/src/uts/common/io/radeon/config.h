/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The OpenIndiana SPARC desktop contributors
 *
 * The Kconfig of radeon on illumos: what Linux's radeon.ko sees from its
 * .config. Everything not defined here is off (IS_ENABLED() false): no
 * ACPI, AGP, VGA switcheroo, MMU notifier, fbdev emulation, debugfs or
 * tracepoints. No x86 platform options, so the same file serves sparcv9.
 */
#ifndef _RADEON_ILLUMOS_CONFIG_H
#define	_RADEON_ILLUMOS_CONFIG_H

#define	CONFIG_64BIT	1
#define	CONFIG_MMU	1
#define	CONFIG_PCI	1
#define	CONFIG_SMP	1
#define	CONFIG_DRM_RADEON	1
#if defined(__x86_64__)
#define	CONFIG_X86	1
#define	CONFIG_X86_64	1
#endif

#endif
