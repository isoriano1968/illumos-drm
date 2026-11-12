/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's console lock (fbdev suspend). illumos's console is not a Linux
 * console: nothing to lock; console suspend stays enabled.
 */
#ifndef _LINUX_CONSOLE_H
#define	_LINUX_CONSOLE_H
#include <linux/types.h>
#define	console_suspend_enabled	true
#define	console_lock()		((void)0)
#define	console_unlock()	((void)0)
#define	console_trylock()	1
#endif
