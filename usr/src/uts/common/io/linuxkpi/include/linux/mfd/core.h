/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's multi-function device cells (amdgpu registers the audio
 * co-processor ACP and the camera ISP of APUs as child platform devices).
 * Not supported: adding cells fails with -ENODEV; the GPUs this port covers
 * have neither block.
 */
#ifndef _LINUX_MFD_CORE_H
#define	_LINUX_MFD_CORE_H
#include <linux/types.h>
#include <linux/errno.h>
struct device;
struct resource;
struct irq_domain;
struct mfd_cell {
	const char *name;
	int id;
	int num_resources;
	const struct resource *resources;
	const void *platform_data;
	size_t pdata_size;
	bool ignore_resource_conflicts;
};
static inline int
mfd_add_devices(struct device *parent, int id, const struct mfd_cell *cells,
    int n_devs, struct resource *mem_base, int irq_base,
    struct irq_domain *irq_domain)
{
	return (-ENODEV);
}
static inline int
mfd_add_hotplug_devices(struct device *parent, const struct mfd_cell *cells,
    int n_devs)
{
	return (-ENODEV);
}
static inline void mfd_remove_devices(struct device *parent) {}
#endif
