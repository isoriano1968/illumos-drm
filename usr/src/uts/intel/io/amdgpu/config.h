/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * The CONFIG_* options amdgpu, TTM and the GPU scheduler test, for the
 * illumos build (first cut, by hand; to be generated like i915's from a
 * Linux configuration once the TU set is settled). Facilities the
 * LinuxKPI layer refuses (lkpi/config.h: DEBUG_FS, PM, LOCKDEP,
 * TRACEPOINTS, PERF_EVENTS, ACPI, MMU_NOTIFIER, COMPAT) stay off.
 */
#ifndef _AMDGPU_ILLUMOS_CONFIG_H
#define	_AMDGPU_ILLUMOS_CONFIG_H

#define	CONFIG_64BIT	1
#define	CONFIG_MMU	1
#define	CONFIG_PCI	1
#define	CONFIG_SMP	1
#define	CONFIG_X86	1
#define	CONFIG_X86_64	1

#define	CONFIG_DRM_AMDGPU	1
#define	CONFIG_DRM_AMDGPU_SI	1
#define	CONFIG_DRM_TTM	1
#define	CONFIG_DRM_SCHED	1
#define	CONFIG_DRM_EXEC	1
#define	CONFIG_DRM_BUDDY	1
#define	CONFIG_DRM_SUBALLOC_HELPER	1
#define	CONFIG_DRM_TTM_HELPER	1
#define	CONFIG_DRM_DISPLAY_HELPER	1
#define	CONFIG_DRM_DISPLAY_DP_HELPER	1
#define	CONFIG_DRM_DISPLAY_HDMI_HELPER	1
#define	CONFIG_DRM_KMS_HELPER	1

#endif
