/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * amdgpu's PRIME on illumos, in place of Linux's amdgpu_dma_buf.c (MIT),
 * which builds amdgpu's own dma_buf_ops (dynamic attachments, peer-to-peer
 * VRAM, move notification). The DRM core exports and imports dma-bufs
 * itself, so:
 *
 * export	the core's: the facade's object callbacks have no .export
 *		(patch 0005), amdgpu_gem_prime_export() is not built.
 *		Another device maps amdgpu's objects through the core's
 *		PRIME maps (drm_illumos_prime_map()): amdgpu_illumos_prime_pin()
 *		pins the object in GTT, as Linux's amdgpu_dma_buf_pin() does
 *		for an importer without peer-to-peer, and hands out the kernel
 *		mapping of its tt pages (LinuxKPI's one DMA binding per tt).
 *		Registered from amdgpu_pci_probe() (patch 0013).
 * import	the core resolves a dma-buf this device exported to the
 *		exporting object and calls the driver's gem_prime_import only
 *		for another device's buffer. That one becomes a ttm_bo_type_sg
 *		object on the buffer's reservation, attached with
 *		dma_buf_attach() (the import half of Linux's amdgpu_dma_buf.c,
 *		amdgpu_dma_buf_create_obj()); its tt maps the attachment when it
 *		is bound (amdgpu_ttm.c, unchanged) and amdgpu_bo_destroy()'s
 *		drm_prime_gem_destroy() detaches. The attachment is static: the
 *		buffer stays pinned while mapped, so no move notification.
 * XGMI		no hive shares memory through an import: false.
 * amdgpu_dmabuf_ops
 *	KFD's test for "an amdgpu dma-buf" (amdgpu_amdkfd.c): empty and never
 *	any dma-buf's ops (the core's dma-bufs have none), so KFD sees no
 *	amdgpu dma-buf.
 */
#include <linux/err.h>
#include <linux/dma-buf.h>
#include <drm/drm_gem.h>
#include <drm/ttm/ttm_tt.h>
#include <lkpi/dma_pages.h>
#include <lkpi/prime.h>
#include "amdgpu.h"
#include "amdgpu_gem.h"
#include "amdgpu_dma_buf.h"
#include "amdgpu_ttm.h"

/* --- export: another device maps one of amdgpu's objects --- */

/* With obj->resv held (by the importer, drm_illumos_prime_map()). */
static int
amdgpu_illumos_prime_pin(struct drm_gem_object *obj, void **kaddr)
{
	struct amdgpu_bo *bo;
	struct ttm_tt *ttm;
	void *k;
	int r;

	if (obj->funcs != &amdgpu_gem_object_funcs)
		return (-EINVAL);
	bo = gem_to_amdgpu_bo(obj);
	/* As amdgpu_gem_prime_export() refuses these. */
	if (amdgpu_ttm_tt_get_usermm(bo->tbo.ttm) != NULL ||
	    (bo->flags & AMDGPU_GEM_CREATE_VM_ALWAYS_VALID) != 0)
		return (-EPERM);
	/* An import has no pages of its own: its dma-buf is the exporter's. */
	if (bo->tbo.type == ttm_bo_type_sg)
		return (-ENOTSUP);
	r = amdgpu_bo_pin(bo, AMDGPU_GEM_DOMAIN_GTT);
	if (r != 0)
		return (r);
	ttm = bo->tbo.ttm;
	k = ttm != NULL ? lkpi_dma_pages_kva(ttm->illumos_dma) : NULL;
	if (k == NULL) {
		/* Only the plain pool binds a tt in one piece (patch 0011). */
		amdgpu_bo_unpin(bo);
		return (-ENOTSUP);
	}
	*kaddr = k;
	return (0);
}

static void
amdgpu_illumos_prime_unpin(struct drm_gem_object *obj)
{
	amdgpu_bo_unpin(gem_to_amdgpu_bo(obj));
}

static const struct drm_illumos_prime_export_funcs amdgpu_illumos_prime_funcs = {
	.pin = amdgpu_illumos_prime_pin,
	.unpin = amdgpu_illumos_prime_unpin,
};

/* From amdgpu_pci_probe() (patch 0013), once the DRM device exists. */
void
amdgpu_illumos_prime_init(struct drm_device *ddev)
{
	if (lkpi_drm_set_prime_export(ddev, &amdgpu_illumos_prime_funcs) != 0)
		drm_warn(ddev, "PRIME exports to other devices are off\n");
}

/* --- import: another device's buffer becomes an amdgpu object --- */

/*
 * Linux's amdgpu_dma_buf_create_obj() (amdgpu_dma_buf.c, MIT): an object
 * of type sg on the dma-buf's reservation, GTT only. Another amdgpu
 * device's buffer passes its caching flags on, as Linux does for
 * amdgpu_dmabuf_ops (here: an object with amdgpu's GEM functions).
 */
static struct drm_gem_object *
amdgpu_illumos_dma_buf_create_obj(struct drm_device *dev,
    struct dma_buf *dma_buf)
{
	struct dma_resv *resv = dma_buf->resv;
	struct amdgpu_device *adev = drm_to_adev(dev);
	struct drm_gem_object *other = dma_buf->priv;
	struct drm_gem_object *gobj;
	struct amdgpu_bo *bo;
	uint64_t flags = 0;
	int ret;

	dma_resv_lock(resv, NULL);
	if (other != NULL && other->funcs == &amdgpu_gem_object_funcs) {
		flags |= gem_to_amdgpu_bo(other)->flags &
		    (AMDGPU_GEM_CREATE_CPU_GTT_USWC | AMDGPU_GEM_CREATE_COHERENT |
		    AMDGPU_GEM_CREATE_EXT_COHERENT | AMDGPU_GEM_CREATE_UNCACHED);
	}
	ret = amdgpu_gem_object_create(adev, dma_buf->size, PAGE_SIZE,
	    AMDGPU_GEM_DOMAIN_CPU, flags, ttm_bo_type_sg, resv, &gobj, 0);
	if (ret != 0) {
		dma_resv_unlock(resv);
		return (ERR_PTR(ret));
	}
	bo = gem_to_amdgpu_bo(gobj);
	bo->allowed_domains = AMDGPU_GEM_DOMAIN_GTT;
	bo->preferred_domains = AMDGPU_GEM_DOMAIN_GTT;
	dma_resv_unlock(resv);
	return (gobj);
}

struct drm_gem_object *
amdgpu_gem_prime_import(struct drm_device *dev, struct dma_buf *dma_buf)
{
	struct drm_gem_object *obj = dma_buf->priv;
	struct dma_buf_attachment *attach;

	if (obj != NULL && obj->dev == dev &&
	    obj->funcs == &amdgpu_gem_object_funcs) {
		drm_gem_object_get(obj);
		return (obj);
	}
	obj = amdgpu_illumos_dma_buf_create_obj(dev, dma_buf);
	if (IS_ERR(obj))
		return (obj);
	attach = dma_buf_attach(dma_buf, dev->dev);
	if (IS_ERR(attach)) {
		drm_gem_object_put(obj);
		return (ERR_CAST(attach));
	}
	get_dma_buf(dma_buf);
	obj->import_attach = attach;
	return (obj);
}

const struct dma_buf_ops amdgpu_dmabuf_ops = { 0 };

bool
amdgpu_dmabuf_is_xgmi_accessible(struct amdgpu_device *adev,
    struct amdgpu_bo *bo)
{
	(void) adev;
	(void) bo;
	return (false);
}
