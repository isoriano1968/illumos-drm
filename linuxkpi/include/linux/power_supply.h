/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's power supply class. amdgpu asks whether the system runs on AC
 * power to pick its power state; illumos reports AC (desktops and the Mac
 * Pro have no battery).
 */
#ifndef _LINUX_POWER_SUPPLY_H
#define	_LINUX_POWER_SUPPLY_H
static inline int
power_supply_is_system_supplied(void)
{
	return (1);
}
#endif
