/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's reboot and power-off requests. amdgpu calls orderly_poweroff()
 * when the GPU overheats past its shutdown limit; on illumos this logs
 * the request and does not power the system off (the platform's own
 * thermal protection stays in charge).
 */
#ifndef _LINUX_REBOOT_H
#define	_LINUX_REBOOT_H
#include <linux/types.h>
#include <linux/printk.h>

static inline int
orderly_poweroff(bool force)
{
	pr_crit("orderly_poweroff requested (GPU thermal shutdown); "
	    "not powering off on illumos\n");
	return (0);
}

/*
 * An immediate reboot (amdgpu: after a fatal RAS error, when its "reboot"
 * policy is set). Logged only, as orderly_poweroff(): a driver does not
 * reboot illumos.
 */
static inline void
emergency_restart(void)
{
	pr_emerg("emergency_restart requested; not rebooting on illumos\n");
}
#endif
