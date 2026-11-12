/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's GEM framebuffer helpers (drm_gem_framebuffer_helper.c is
 * GPL-2.0-or-later and its header carries no licence): the interface
 * amdgpu uses, to be implemented on the facade's GEM by the illumos port
 * (to do).
 */
#ifndef __DRM_GEM_FB_HELPER_H__
#define	__DRM_GEM_FB_HELPER_H__
#include <linux/types.h>
struct drm_file;
struct drm_framebuffer;
struct drm_gem_object;
struct drm_device;
struct drm_mode_fb_cmd2;
struct drm_framebuffer_funcs;
extern struct drm_gem_object *drm_gem_fb_get_obj(struct drm_framebuffer *fb,
    unsigned int plane);
extern void drm_gem_fb_destroy(struct drm_framebuffer *fb);
extern int drm_gem_fb_create_handle(struct drm_framebuffer *fb,
    struct drm_file *file, unsigned int *handle);
#endif
