/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's GEM helpers for TTM objects (drm_gem_ttm_helper.h and .c are
 * GPL-2.0-or-later): the interface, reimplemented by the illumos port
 * (drivers/amdgpu/illumos/drm_gem_ttm_illumos.c) on the facade's GEM and
 * the carried TTM.
 */
#ifndef _DRM_GEM_TTM_HELPER_H
#define	_DRM_GEM_TTM_HELPER_H
#include <linux/kernel.h>
#include <drm/drm_gem.h>
#include <drm/drm_device.h>
#include <drm/ttm/ttm_bo.h>

struct drm_printer;
struct iosys_map;
struct vm_area_struct;

#define	drm_gem_ttm_of_gem(gem_obj) \
	container_of(gem_obj, struct ttm_buffer_object, base)

extern void drm_gem_ttm_print_info(struct drm_printer *p, unsigned int indent,
    const struct drm_gem_object *gem);
extern int drm_gem_ttm_vmap(struct drm_gem_object *gem, struct iosys_map *map);
extern void drm_gem_ttm_vunmap(struct drm_gem_object *gem,
    struct iosys_map *map);
extern int drm_gem_ttm_mmap(struct drm_gem_object *gem,
    struct vm_area_struct *vma);
extern int drm_gem_ttm_dumb_map_offset(struct drm_file *file,
    struct drm_device *dev, uint32_t handle, uint64_t *offset);
#endif
