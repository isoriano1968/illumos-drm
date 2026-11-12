/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Apple's gmux (the display multiplexer of dual-GPU MacBook Pros; Linux's
 * header is GPL-2.0-only). Not supported: no gmux is detected, so amdgpu
 * treats the GPU as the only one driving its outputs. TO CHECK on the
 * Mac Pro 2013 (MacPro6,1) whether it has a gmux (Linux lists the Mac Pro
 * in the header's description).
 */
#ifndef _LINUX_APPLE_GMUX_H
#define	_LINUX_APPLE_GMUX_H
#include <linux/types.h>
struct pnp_dev;
enum apple_gmux_type { APPLE_GMUX_TYPE_PIO, APPLE_GMUX_TYPE_INDEXED, APPLE_GMUX_TYPE_MMIO };
static inline bool
apple_gmux_detect(struct pnp_dev *pnp_dev, enum apple_gmux_type *type_ret)
{
	return (false);
}
#define	apple_gmux_present()	false
#endif
