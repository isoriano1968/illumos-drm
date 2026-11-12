/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * PRIME exports of a LinuxKPI driver's private objects to other devices
 * (illumos-specific). The layer gives the DRM core its own callbacks when
 * it registers the device (devm_drm_dev_alloc()); they call the ones the
 * driver sets here, any time after allocation (struct
 * drm_illumos_prime_export_funcs, <drm/dma_buf.h>: pin returns a kernel
 * address of the whole object, with obj->resv held). Until then, or for a
 * driver that never sets them, other devices cannot map its objects
 * (-ENOTSUP). Returns 0 or -EINVAL.
 */
#ifndef _LKPI_PRIME_H
#define	_LKPI_PRIME_H

#include <drm/dma_buf.h>

struct drm_device;

extern int lkpi_drm_set_prime_export(struct drm_device *dev,
    const struct drm_illumos_prime_export_funcs *funcs);

#endif	/* _LKPI_PRIME_H */
