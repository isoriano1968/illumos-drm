/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The OpenIndiana SPARC desktop contributors
 *
 * User mappings of radeon buffer objects through the DRM core, after
 * illumos-drm's amdgpu_illumos_mmap.c, in place of Linux's
 * radeon_gem_fault()/ttm_bo_vm_fault(). An object shows one of two views
 * to all its user mappings:
 *
 * - RAM: the object's user-mapping window (<lkpi/umem_window.h>), kernel
 *   VA with a umem cookie over the object's TT pages, created at the
 *   first mmap(2) (illumos_umem).
 * - VRAM: BAR 0 (the VRAM aperture) at the object's VRAM offset,
 *   write-combined, with devmap_devmem_remap(9F). radeon's VRAM manager
 *   (ttm_range) keeps every object contiguous.
 *
 * A fault (illumos_access; the objects are movable) reserves the object
 * and brings it where the CPU can see it, as
 * radeon_bo_fault_reserve_notify() does: VRAM beyond the visible part
 * moves to visible VRAM, else to GTT. It waits for the move, prepares the
 * view, points every piece at it if it changed, and loads the faulting
 * range, all with the reservation held. The rules and the lock order are
 * amdgpu's (see there).
 *
 * ttm_bo_illumos_unmap_virtual(): TTM's CPU-mapping teardown before pages
 * or VRAM move or go: every user mapping piece is unloaded, then the
 * window. radeon_illumos_bo_fini(): radeon_ttm_bo_destroy() frees the
 * window.
 */

#include <sys/mman.h>		/* PROT_ALL */
#include <linux/dma-resv.h>
#include <linux/pci.h>
#include <lkpi/umem_window.h>
#include <drm/ttm/ttm_bo.h>
#include <drm/ttm/ttm_tt.h>

#include "radeon.h"
#include "radeon_object.h"
#include <drm/drm_illumos.h>	/* after drm_gem.h: its structs */

ddi_umem_cookie_t
radeon_illumos_gem_umem(struct drm_gem_object *gobj)
{
	struct radeon_bo *bo = gem_to_radeon_bo(gobj);
	struct lkpi_umem_window *w = READ_ONCE(bo->illumos_window);

	if (w == NULL) {
		w = lkpi_umem_window_create(radeon_bo_size(bo));
		if (w == NULL)
			return (NULL);
		/* Calls are serialized by the mapping lock; be safe anyway. */
		if (cmpxchg(&bo->illumos_window, NULL, w) != NULL) {
			lkpi_umem_window_destroy(w);
			w = READ_ONCE(bo->illumos_window);
		}
	}
	return (lkpi_umem_window_cookie(w));
}

static int
radeon_illumos_fault_errno(int r)
{
	switch (r) {
	case -EINTR:
	case -ERESTARTSYS:
		return (EINTR);
	case -ENOMEM:
		return (ENOMEM);
	default:
		return (EFAULT);
	}
}

/* A new piece (mmap, fork, split); the core's mapping lock is held. */
int
radeon_illumos_gem_mapped(struct drm_gem_object *gobj,
    struct drm_illumos_mapping *map)
{
	struct radeon_bo *bo = gem_to_radeon_bo(gobj);

	(void) map;
	/* An mmap(2) piece shows the window: repaired at the next fault. */
	if (bo->illumos_view_vram)
		bo->illumos_remap_pending = true;
	return (0);
}

struct radeon_illumos_view {
	struct radeon_bo	*bo;
	bool			vram;
	u64			bar_off;
	int			err;
};

static void
radeon_illumos_point_piece(struct drm_illumos_mapping *map, void *arg)
{
	struct radeon_illumos_view *v = arg;
	struct radeon_bo *bo = v->bo;
	struct pci_dev *pdev = bo->rdev->pdev;
	devmap_cookie_t dhp = drm_illumos_mapping_cookie(map);
	offset_t ooff = (offset_t)drm_illumos_mapping_object_offset(map);
	size_t len = drm_illumos_mapping_len(map);
	ddi_device_acc_attr_t acc = {
		.devacc_attr_version = DDI_DEVICE_ATTR_V0,
		.devacc_attr_endian_flags = DDI_NEVERSWAP_ACC,
		.devacc_attr_dataorder = DDI_STRICTORDER_ACC,
	};
	int r;

	bo->illumos_view_vram = v->vram;
	bo->illumos_bar_off = v->bar_off;
	bo->illumos_remap_pending = false;

	if (v->vram) {
		acc.devacc_attr_dataorder = DDI_MERGING_OK_ACC;
		r = devmap_devmem_remap(dhp, pdev->dev.lkpi_dip,
		    (uint_t)pdev->lkpi_regnum[0],
		    (offset_t)v->bar_off + ooff, len, PROT_ALL, 0, &acc);
	} else {
		r = devmap_umem_remap(dhp, pdev->dev.lkpi_dip,
		    lkpi_umem_window_cookie(bo->illumos_window), ooff, len,
		    PROT_ALL, 0, &acc);
	}
	if (r != DDI_SUCCESS && v->err == 0) {
		v->err = EFAULT;
		bo->illumos_remap_pending = true;
	}
}

static bool
radeon_illumos_bar_mappable(struct radeon_device *rdev,
    struct ttm_resource *res)
{
	return (res != NULL && res->mem_type == TTM_PL_VRAM &&
	    rdev->pdev->lkpi_regnum[0] >= 0 &&
	    ((u64)res->start << PAGE_SHIFT) + res->size <=
	    rdev->mc.visible_vram_size);
}

/* radeon_bo_fault_reserve_notify(), with the reservation held. */
static int
radeon_illumos_place_for_cpu(struct radeon_device *rdev,
    struct radeon_bo *rbo, struct ttm_operation_ctx *ctx)
{
	struct ttm_buffer_object *bo = &rbo->tbo;
	struct ttm_resource *res = bo->resource;
	unsigned long lpfn;
	int i, r;

	if (res != NULL && (res->mem_type == TTM_PL_TT ||
	    res->mem_type == TTM_PL_SYSTEM))
		return (0);
	if (radeon_illumos_bar_mappable(rdev, res))
		return (0);
	if (bo->pin_count > 0)
		return (-EFAULT);

	radeon_ttm_placement_from_domain(rbo, RADEON_GEM_DOMAIN_VRAM);
	lpfn = rdev->mc.visible_vram_size >> PAGE_SHIFT;
	for (i = 0; i < rbo->placement.num_placement; i++) {
		if (rbo->placements[i].mem_type == TTM_PL_VRAM &&
		    (!rbo->placements[i].lpfn ||
		    rbo->placements[i].lpfn > lpfn))
			rbo->placements[i].lpfn = lpfn;
	}
	r = ttm_bo_validate(bo, &rbo->placement, ctx);
	if (r == -ENOMEM) {
		radeon_ttm_placement_from_domain(rbo, RADEON_GEM_DOMAIN_GTT);
		r = ttm_bo_validate(bo, &rbo->placement, ctx);
	}
	return (r);
}

int
radeon_illumos_gem_access(struct drm_gem_object *gobj,
    struct drm_illumos_mapping *map, struct drm_illumos_fault *fault)
{
	struct radeon_bo *rbo = gem_to_radeon_bo(gobj);
	struct radeon_device *rdev = rbo->rdev;
	struct ttm_buffer_object *bo = &rbo->tbo;
	struct ttm_operation_ctx ctx = { .interruptible = true };
	struct lkpi_umem_window *w = READ_ONCE(rbo->illumos_window);
	struct radeon_illumos_view v = { .bo = rbo };
	long lret;
	int r;

	(void) map;
	if (w == NULL || bo->type == ttm_bo_type_sg)
		return (EFAULT);
	r = dma_resv_lock_interruptible(bo->base.resv, NULL);
	if (r != 0)
		return (radeon_illumos_fault_errno(r));

	r = radeon_illumos_place_for_cpu(rdev, rbo, &ctx);
	if (r != 0)
		goto out;
	lret = dma_resv_wait_timeout(bo->base.resv, DMA_RESV_USAGE_KERNEL,
	    true, MAX_SCHEDULE_TIMEOUT);
	if (lret < 0) {
		r = (int)lret;
		goto out;
	}

	if (bo->resource != NULL && bo->resource->mem_type == TTM_PL_VRAM) {
		if (!radeon_illumos_bar_mappable(rdev, bo->resource)) {
			r = -EFAULT;
			goto out;
		}
		v.vram = true;
		v.bar_off = (u64)bo->resource->start << PAGE_SHIFT;
	} else {
		if (bo->ttm == NULL || bo->ttm->pages == NULL) {
			r = -EFAULT;
			goto out;
		}
		r = ttm_bo_populate(bo, &ctx);
		if (r != 0)
			goto out;
		if (!lkpi_umem_window_loaded(w)) {
			r = lkpi_umem_window_load(w, bo->ttm->pages,
			    bo->ttm->num_pages);
			if (r != 0)
				goto out;
		}
	}

	if (v.vram != rbo->illumos_view_vram ||
	    (v.vram && v.bar_off != rbo->illumos_bar_off) ||
	    READ_ONCE(rbo->illumos_remap_pending)) {
		drm_illumos_gem_foreach_mapping(gobj,
		    radeon_illumos_point_piece, &v);
		if (v.err != 0) {
			dma_resv_unlock(bo->base.resv);
			return (v.err);
		}
	}

	r = drm_illumos_fault_load(fault);
	dma_resv_unlock(bo->base.resv);
	return (r);
out:
	dma_resv_unlock(bo->base.resv);
	return (radeon_illumos_fault_errno(r));
}

static void
radeon_illumos_unload_piece(struct drm_illumos_mapping *map, void *arg)
{
	int r = drm_illumos_mapping_unload(map);

	(void) arg;
	WARN_ONCE(r != 0, "radeon: user mapping not unloaded (%d)", r);
}

void
ttm_bo_illumos_unmap_virtual(struct ttm_buffer_object *tbo)
{
	struct radeon_bo *bo;
	struct lkpi_umem_window *w;

	if (!radeon_ttm_bo_is_radeon_bo(tbo))
		return;
	bo = container_of(tbo, struct radeon_bo, tbo);
	w = READ_ONCE(bo->illumos_window);
	if (w == NULL)
		return;		/* never mapped */
	drm_illumos_gem_foreach_mapping(&tbo->base,
	    radeon_illumos_unload_piece, NULL);
	lkpi_umem_window_unload(w);
}

void
radeon_illumos_bo_fini(struct radeon_bo *bo)
{
	lkpi_umem_window_destroy(bo->illumos_window);
	bo->illumos_window = NULL;
}
