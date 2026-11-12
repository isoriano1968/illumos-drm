/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's <linux/memremap.h>: memremap() and MEMREMAP_* are in the layer's
 * <linux/io.h>; ZONE_DEVICE pages (KFD's SVM) are not provided.
 */
#ifndef _LINUX_MEMREMAP_H
#define	_LINUX_MEMREMAP_H
#include <linux/io.h>
#include <linux/types.h>
#ifndef _LINUX_RANGE_H
#define	_LINUX_RANGE_H
struct range {
	u64 start;
	u64 end;
};
#endif
/*
 * ZONE_DEVICE page maps (KFD's SVM migration). Not provided: the structure
 * exists because amdgpu embeds one; nothing maps device pages.
 */
enum memory_type { MEMORY_DEVICE_PRIVATE = 1, MEMORY_DEVICE_COHERENT,
	MEMORY_DEVICE_FS_DAX, MEMORY_DEVICE_GENERIC, MEMORY_DEVICE_PCI_P2PDMA };
struct dev_pagemap_ops;
struct dev_pagemap {
	enum memory_type type;
	unsigned int flags;
	unsigned long vmemmap_shift;
	const struct dev_pagemap_ops *ops;
	void *owner;
	int nr_range;
	union {
		struct range range;
		struct { struct { } __empty_ranges; struct range ranges[]; };
	};
};
#endif
