/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's 32-bit compatibility types (include/linux/compat.h, GPL-2.0;
 * reimplemented). illumos drivers see 32-bit callers through
 * ddi_model_convert_from(9F); the DRM core does that, so CONFIG_COMPAT
 * stays off and only the types are here.
 */
#ifndef _LINUX_COMPAT_H
#define	_LINUX_COMPAT_H

#include <linux/types.h>

typedef u32 compat_uptr_t;
typedef s32 compat_int_t;
typedef u32 compat_uint_t;
typedef s32 compat_long_t;
typedef u32 compat_ulong_t;
typedef u32 compat_size_t;

static inline void __user *
compat_ptr(compat_uptr_t p)
{
	return ((void __user *)(uintptr_t)p);
}

#endif
