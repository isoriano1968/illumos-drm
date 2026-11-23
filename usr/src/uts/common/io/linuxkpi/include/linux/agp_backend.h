/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * AGP: not supported (no AGP on the GPUs this layer serves).
 */
#ifndef _LINUX_AGP_BACKEND_H
#define	_LINUX_AGP_BACKEND_H
#include <linux/types.h>

struct agp_bridge_data;
struct agp_memory;
struct pci_dev;
struct vm_operations_struct;

/* Shapes only: radeon embeds agp_kern_info in its AGP state (unused). */
enum chipset_type {
	NOT_SUPPORTED,
	SUPPORTED,
};

struct agp_version {
	u16 major;
	u16 minor;
};

struct agp_kern_info {
	struct agp_version version;
	struct pci_dev *device;
	enum chipset_type chipset;
	unsigned long mode;
	unsigned long aper_base;
	size_t aper_size;
	int max_memory;
	int current_memory;
	bool cant_use_aperture;
	unsigned long page_mask;
	const struct vm_operations_struct *vm_ops;
};
#endif
