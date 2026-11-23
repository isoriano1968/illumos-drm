/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The OpenIndiana SPARC desktop contributors
 *
 * What radeon needs from Linux code that is not carried on illumos:
 *
 * drm_gem_fb_destroy(), drm_gem_fb_create_handle(): radeon's framebuffer
 * functions (radeon_display.c), from drm_gem_framebuffer_helper.c, which
 * is not carried; the same two as in illumos-drm's amdgpu_illumos_kms.c.
 *
 * ttm_bo_illumos_unmap_virtual(): the illumos replacement of TTM's CPU
 * mapping teardown (illumos-drm's ttm_bo.c patch). radeon's objects have
 * no user mappings on illumos yet (radeon_gem.c sets no mapping
 * callbacks), so there is nothing to tear down. When the mapping path
 * comes (after amdgpu_illumos_mmap.c), this unloads it.
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/ttm/ttm_bo.h>

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

void
ttm_bo_illumos_unmap_virtual(struct ttm_buffer_object *bo)
{
	(void) bo;	/* no user mappings yet */
}
