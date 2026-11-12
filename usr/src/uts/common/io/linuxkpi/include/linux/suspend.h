/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's system sleep states. Not supported: the layer never suspends a
 * device; pm_suspend_target_state is 0 (PM_SUSPEND_ON).
 */
#ifndef _LINUX_SUSPEND_H
#define	_LINUX_SUSPEND_H
#include <linux/pm.h>
#include <linux/notifier.h>
#include <linux/syscalls.h>	/* ksys_sync_helper(), as Linux's suspend.h */

/* PM notifier events (never sent: register_pm_notifier() is a no-op). */
#define	PM_HIBERNATION_PREPARE	0x0001
#define	PM_POST_HIBERNATION	0x0002
#define	PM_SUSPEND_PREPARE	0x0003
#define	PM_POST_SUSPEND		0x0004
#define	PM_RESTORE_PREPARE	0x0005
#define	PM_POST_RESTORE		0x0006
static inline int register_pm_notifier(struct notifier_block *nb) { return 0; }
static inline int unregister_pm_notifier(struct notifier_block *nb) { return 0; }
#define	pm_suspend_default_s2idle()	false
#define	pm_hibernate_is_recovering()	false
#define	pm_hibernation_mode_is_suspend()	false
#endif
