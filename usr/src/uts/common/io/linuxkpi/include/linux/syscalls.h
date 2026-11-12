/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's in-kernel system call helpers. amdgpu's RAS code syncs file
 * systems before a fatal GPU error reset (ksys_sync_helper()); on illumos
 * it does not (no in-kernel sync for drivers).
 */
#ifndef _LINUX_SYSCALLS_H
#define	_LINUX_SYSCALLS_H
#define	ksys_sync_helper()	((void)0)
#endif
