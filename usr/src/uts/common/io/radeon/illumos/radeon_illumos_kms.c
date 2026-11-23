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
 * (ttm_bo_illumos_unmap_virtual() is in radeon_illumos_mmap.c.)
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_framebuffer_helper.h>

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
