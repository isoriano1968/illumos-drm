/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's kernel memory leak detector hooks: no-ops (illumos has
 * ::findleaks in mdb instead).
 */
#ifndef _LINUX_KMEMLEAK_H
#define	_LINUX_KMEMLEAK_H
#include <linux/types.h>
#define	kmemleak_alloc(p, s, c, g)	((void)(p))
#define	kmemleak_free(p)		((void)(p))
#define	kmemleak_not_leak(p)		((void)(p))
#define	kmemleak_ignore(p)		((void)(p))
#define	kmemleak_update_trace(p)	((void)(p))
#endif
