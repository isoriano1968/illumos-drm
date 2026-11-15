/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * What amdgpu needs from Linux modules that are not built for illumos:
 *
 * drm_gem_fb_destroy(), drm_gem_fb_create_handle()
 *	the framebuffer callbacks of Linux's drm_gem_framebuffer_helper.c
 *	(which carries no licence statement, so it is not built):
 *	destroy cleans up the framebuffer, drops the references it holds
 *	on its GEM objects and frees it; create_handle gives the file a
 *	handle for the first plane's object (MODE_GETFB).
 * amdgpu_xcp_drm_dev_alloc(), amdgpu_xcp_drm_dev_free(),
 * amdgpu_xcp_drv_release()
 *	amdxcp.ko, the DRM devices of compute partitions (MI300 class
 *	GPUs). Not built: no partition device can be made (-ENODEV), so a
 *	GPU stays one device; freeing and releasing have nothing to do.
 */
#include <linux/errno.h>
#include <linux/slab.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include "../amdxcp/amdgpu_xcp_drv.h"

void
drm_gem_fb_destroy(struct drm_framebuffer *fb)
{
	unsigned int i;

	drm_framebuffer_cleanup(fb);
	for (i = 0; i < ARRAY_SIZE(fb->obj); i++) {
		if (fb->obj[i] != NULL)
			drm_gem_object_put(fb->obj[i]);
	}
	kfree(fb);
}

int
drm_gem_fb_create_handle(struct drm_framebuffer *fb, struct drm_file *file,
    unsigned int *handle)
{
	if (fb->obj[0] == NULL)
		return (-ENOENT);
	return (drm_gem_handle_create(file, fb->obj[0], handle));
}

int
amdgpu_xcp_drm_dev_alloc(struct drm_device **ddev)
{
	*ddev = NULL;
	return (-ENODEV);
}

void
amdgpu_xcp_drm_dev_free(struct drm_device *ddev)
{
	(void) ddev;
}

void
amdgpu_xcp_drv_release(void)
{
}
