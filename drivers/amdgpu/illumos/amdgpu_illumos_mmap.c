/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * User mappings of amdgpu buffer objects through the DRM core, in place
 * of Linux's amdgpu_gem_fault()/ttm_bo_vm_fault() (ttm_bo_vm.c is not
 * built). An object shows one of two views to all its user mappings:
 *
 * - RAM: the object's user-mapping window (<lkpi/umem_window.h>), kernel
 *   VA with a umem cookie over the object's TT pages, mapped cached as
 *   the core maps every umem cookie. Created at the first mmap(2)
 *   (illumos_umem, with the core's mapping lock held); new mapping
 *   pieces start on it.
 * - VRAM: BAR 0 at the object's VRAM offset, write-combined
 *   (devmap_devmem_remap(9F) with DDI_MERGING_OK_ACC). Only for objects
 *   that lie contiguous in CPU-visible VRAM: a devmem piece maps one
 *   contiguous range of a register set. A umem cookie cannot serve VRAM:
 *   devmap_umem_remap(9F) takes no cache flags, and with the core's
 *   cached attributes a BAR frame would get a write-back entry whose
 *   real type then depends on the firmware's MTRRs.
 *
 * A fault (illumos_access; the object is movable, so every fault comes
 * here, without the core's mapping lock) reserves the object and brings
 * it where the CPU can see it, as Linux's amdgpu_bo_fault_reserve_notify()
 * does: an object in VRAM that is not contiguous or not CPU-visible moves
 * to contiguous visible VRAM if that needs no eviction, else to GTT;
 * objects elsewhere (not VRAM, not system pages) go to GTT. It then waits
 * for the move, prepares the view (populates and loads the window for
 * RAM), points every piece at that view if the view changed (or a piece
 * was added while the view was VRAM), and loads the faulting range, all
 * with the reservation held.
 *
 * The view (struct amdgpu_bo illumos_view_vram, illumos_bar_off) changes
 * only there, with the reservation held and inside
 * drm_illumos_gem_foreach_mapping() (the core's mapping lock held), in
 * the same walk that remaps every piece. The core calls illumos_mapped,
 * with the mapping lock held, for every new piece: mmap(2) pieces start
 * on the window, fork(2) and split pieces are copies of a piece that
 * shows the view. So the invariant "every piece shows the view" can
 * only break through a new mmap(2) piece while the view is VRAM, which
 * illumos_mapped records (illumos_remap_pending) for the next fault to
 * repair; and every fault passes through illumos_access before any
 * translation is loaded.
 *
 * TTM tears CPU mappings down with ttm_bo_unmap_virtual() before pages or
 * VRAM move or go (moves, swap-out, purge); patch 0009 routes that here
 * (ttm_bo_illumos_unmap_virtual(), with the reservation held): every user
 * mapping piece is unloaded, then the window. The view stays; the next
 * fault compares it with where the object is then.
 *
 * GTT pages are kept cached and snooped on illumos (patch 0010,
 * amdgpu_ttm_tt_create()), because the core maps the window write-back.
 *
 * Lock order: reservation, then the core's mapping lock (the unmap and
 * remap walks take it in drm_illumos_gem_foreach_mapping()).
 * illumos_umem and illumos_mapped run with the mapping lock held and take
 * no reservation.
 */

#include <sys/mman.h>		/* PROT_ALL */
#include <linux/dma-resv.h>
#include <lkpi/umem_window.h>
#include <drm/ttm/ttm_bo.h>
#include <drm/ttm/ttm_tt.h>

#include "amdgpu.h"
#include "amdgpu_object.h"
#include "amdgpu_res_cursor.h"
#include <drm/drm_illumos.h>	/* after drm_gem.h: its structs */

/* devmap_access(9E) wants an errno; the TTM paths return -errno. */
static int
amdgpu_illumos_fault_errno(int r)
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

ddi_umem_cookie_t
amdgpu_illumos_gem_umem(struct drm_gem_object *gobj)
{
	struct amdgpu_bo *abo = gem_to_amdgpu_bo(gobj);
	struct lkpi_umem_window *w = READ_ONCE(abo->illumos_window);

	if (w == NULL) {
		w = lkpi_umem_window_create(amdgpu_bo_size(abo));
		if (w == NULL)
			return (NULL);
		/* Calls are serialized by the mapping lock; be safe anyway. */
		if (cmpxchg(&abo->illumos_window, NULL, w) != NULL) {
			lkpi_umem_window_destroy(w);
			w = READ_ONCE(abo->illumos_window);
		}
	}
	return (lkpi_umem_window_cookie(w));
}

/* A new piece (mmap, fork, split); the core's mapping lock is held. */
int
amdgpu_illumos_gem_mapped(struct drm_gem_object *gobj,
    struct drm_illumos_mapping *map)
{
	struct amdgpu_bo *abo = gem_to_amdgpu_bo(gobj);

	(void) map;
	/*
	 * Copies (fork, split) show what their original showed: the view.
	 * An mmap(2) piece shows the window, which is the view unless the
	 * view is VRAM; then the next fault repairs it (it cannot be
	 * repaired here: the object's VRAM offset is only stable under the
	 * reservation, which this callback must not take).
	 */
	if (abo->illumos_view_vram)
		abo->illumos_remap_pending = true;
	return (0);
}

/* The view the remap walk points every piece at. */
struct amdgpu_illumos_view {
	struct amdgpu_bo	*abo;
	bool			vram;
	u64			bar_off;
	int			err;	/* the first failed remap */
};

/*
 * Points one piece at the view (drm_illumos_gem_foreach_mapping(), the
 * mapping lock held). A remap unloads the whole piece; the next access
 * to any page of it faults again.
 */
static void
amdgpu_illumos_point_piece(struct drm_illumos_mapping *map, void *arg)
{
	struct amdgpu_illumos_view *v = arg;
	struct amdgpu_bo *abo = v->abo;
	struct amdgpu_device *adev = amdgpu_ttm_adev(abo->tbo.bdev);
	struct pci_dev *pdev = adev->pdev;
	devmap_cookie_t dhp = drm_illumos_mapping_cookie(map);
	offset_t ooff = (offset_t)drm_illumos_mapping_object_offset(map);
	size_t len = drm_illumos_mapping_len(map);
	ddi_device_acc_attr_t acc = {
		.devacc_attr_version = DDI_DEVICE_ATTR_V0,
		.devacc_attr_endian_flags = DDI_NEVERSWAP_ACC,
		.devacc_attr_dataorder = DDI_STRICTORDER_ACC,
	};
	int r;

	/* The view is the object's from the first piece on (mapping lock). */
	abo->illumos_view_vram = v->vram;
	abo->illumos_bar_off = v->bar_off;
	abo->illumos_remap_pending = false;

	if (v->vram) {
		/* BAR 0 is the VRAM aperture (gmc_v6_0: aper_base). */
		acc.devacc_attr_dataorder = DDI_MERGING_OK_ACC;
		r = devmap_devmem_remap(dhp, pdev->dev.lkpi_dip,
		    (uint_t)pdev->lkpi_regnum[0],
		    (offset_t)v->bar_off + ooff, len, PROT_ALL, 0, &acc);
	} else {
		/* As at setup: the window, PROT_ALL, flags 0 (cached). */
		r = devmap_umem_remap(dhp, pdev->dev.lkpi_dip,
		    lkpi_umem_window_cookie(abo->illumos_window), ooff, len,
		    PROT_ALL, 0, &acc);
	}
	if (r != DDI_SUCCESS && v->err == 0) {
		v->err = EFAULT;
		/* This piece shows something else: repair at the next fault. */
		abo->illumos_remap_pending = true;
	}
}

/* Whether the object is in VRAM where BAR 0 can show it in one piece. */
static bool
amdgpu_illumos_bar_mappable(struct amdgpu_device *adev,
    struct ttm_resource *res)
{
	return (res != NULL && res->mem_type == TTM_PL_VRAM &&
	    (res->placement & TTM_PL_FLAG_CONTIGUOUS) != 0 &&
	    adev->pdev->lkpi_regnum[0] >= 0 &&
	    amdgpu_res_cpu_visible(adev, res));
}

/*
 * Brings the object where the CPU can map it: contiguous visible VRAM,
 * or system pages. With the reservation held.
 */
static int
amdgpu_illumos_place_for_cpu(struct amdgpu_device *adev,
    struct amdgpu_bo *abo, struct ttm_operation_ctx *ctx)
{
	struct ttm_buffer_object *bo = &abo->tbo;
	struct ttm_resource *res = bo->resource;
	u32 domain;

	if (res != NULL && (res->mem_type == TTM_PL_TT ||
	    res->mem_type == TTM_PL_SYSTEM ||
	    res->mem_type == AMDGPU_PL_PREEMPT))
		return (0);
	if (amdgpu_illumos_bar_mappable(adev, res))
		return (0);
	if (bo->pin_count > 0)
		return (-EFAULT);	/* Linux: VM_FAULT_SIGBUS */

	/* Remember that the CPU accesses it: visible VRAM from now on. */
	abo->flags |= AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;
	domain = abo->allowed_domains & AMDGPU_GEM_DOMAIN_GTT;
	if (res != NULL && res->mem_type == TTM_PL_VRAM)
		domain |= AMDGPU_GEM_DOMAIN_VRAM;
	if (domain == 0)
		return (-EFAULT);
	amdgpu_bo_placement_from_domain(abo, domain);
	if (domain & AMDGPU_GEM_DOMAIN_VRAM) {
		/* VRAM comes first (amdgpu_bo_placement_from_domain()). */
		abo->placements[0].flags |= TTM_PL_FLAG_CONTIGUOUS;
		abo->placements[0].lpfn = min_not_zero(abo->placements[0].lpfn,
		    (unsigned)(adev->gmc.visible_vram_size >> PAGE_SHIFT));
		/* As Linux: no eviction for VRAM when GTT will do. */
		if (domain & AMDGPU_GEM_DOMAIN_GTT)
			abo->placements[0].flags |= TTM_PL_FLAG_DESIRED;
	}
	return (ttm_bo_validate(bo, &abo->placement, ctx));
}

int
amdgpu_illumos_gem_access(struct drm_gem_object *gobj,
    struct drm_illumos_mapping *map, struct drm_illumos_fault *fault)
{
	struct amdgpu_bo *abo = gem_to_amdgpu_bo(gobj);
	struct amdgpu_device *adev = amdgpu_ttm_adev(abo->tbo.bdev);
	struct ttm_buffer_object *bo = &abo->tbo;
	struct ttm_operation_ctx ctx = { .interruptible = true };
	struct lkpi_umem_window *w = READ_ONCE(abo->illumos_window);
	struct amdgpu_illumos_view v = { .abo = abo };
	struct amdgpu_res_cursor cursor;
	long lret;
	int r;

	(void) map;
	/*
	 * An import (ttm_bo_type_sg) has only the exporter's DMA addresses,
	 * no pages: no CPU mapping, as Linux's TTM fault gives SIGBUS for an
	 * external tt that is not mappable.
	 */
	if (w == NULL || bo->type == ttm_bo_type_sg)
		return (EFAULT);
	r = dma_resv_lock_interruptible(bo->base.resv, NULL);
	if (r != 0)
		return (amdgpu_illumos_fault_errno(r));

	r = amdgpu_illumos_place_for_cpu(adev, abo, &ctx);
	if (r != 0)
		goto out;
	/* A move (a copy by the GPU) must be done before the CPU looks. */
	lret = dma_resv_wait_timeout(bo->base.resv, DMA_RESV_USAGE_KERNEL,
	    true, MAX_SCHEDULE_TIMEOUT);
	if (lret < 0) {
		r = (int)lret;
		goto out;
	}

	if (bo->resource != NULL && bo->resource->mem_type == TTM_PL_VRAM) {
		if (!amdgpu_illumos_bar_mappable(adev, bo->resource)) {
			r = -EFAULT;	/* "should never happen" (Linux) */
			goto out;
		}
		amdgpu_res_first(bo->resource, 0, amdgpu_bo_size(abo),
		    &cursor);
		v.vram = true;
		v.bar_off = cursor.start;
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

	/* Point every piece at the view if it changed or a piece lags. */
	if (v.vram != abo->illumos_view_vram ||
	    (v.vram && v.bar_off != abo->illumos_bar_off) ||
	    READ_ONCE(abo->illumos_remap_pending)) {
		drm_illumos_gem_foreach_mapping(gobj,
		    amdgpu_illumos_point_piece, &v);
		if (v.err != 0) {
			dma_resv_unlock(bo->base.resv);
			return (v.err);
		}
	}

	/* Loaded with the reservation held: no move can come in between. */
	r = drm_illumos_fault_load(fault);
	dma_resv_unlock(bo->base.resv);
	return (r);
out:
	dma_resv_unlock(bo->base.resv);
	return (amdgpu_illumos_fault_errno(r));
}

static void
amdgpu_illumos_unload_piece(struct drm_illumos_mapping *map, void *arg)
{
	int r = drm_illumos_mapping_unload(map);

	(void) arg;
	WARN_ONCE(r != 0, "amdgpu: user mapping not unloaded (%d)", r);
}

/*
 * ttm_bo_unmap_virtual() on illumos (patch 0009): the object's pages or
 * VRAM are about to move or go. The user mappings first, so that no
 * fault can find the window loaded with the old pages or a piece showing
 * the old VRAM, then the window. With the reservation held (TTM's
 * callers hold it), so no fault runs meanwhile.
 */
void
ttm_bo_illumos_unmap_virtual(struct ttm_buffer_object *bo)
{
	struct amdgpu_bo *abo;
	struct lkpi_umem_window *w;

	if (!amdgpu_bo_is_amdgpu_bo(bo))
		return;
	abo = ttm_to_amdgpu_bo(bo);
	w = READ_ONCE(abo->illumos_window);
	if (w == NULL)
		return;		/* never mapped */
	drm_illumos_gem_foreach_mapping(&bo->base,
	    amdgpu_illumos_unload_piece, NULL);
	lkpi_umem_window_unload(w);
}

/* amdgpu_bo_destroy() (patch 0010): no mapping is left. */
void
amdgpu_illumos_bo_fini(struct amdgpu_bo *abo)
{
	lkpi_umem_window_destroy(abo->illumos_window);
	abo->illumos_window = NULL;
}
