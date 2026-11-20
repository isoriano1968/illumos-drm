/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * The KMS side of the illumos console on amdgpu (DCE6 and other legacy,
 * non-atomic display engines), after i915n's i915_illumos_con_kms.c.
 *
 * The console framebuffer is the loader's (fb_info): the UEFI GOP left it
 * in VRAM, gfx_private draws the console into it through the VRAM BAR,
 * and the display keeps scanning it out after attach until a DRM client
 * (X) sets a mode. Patch 0017 reserves it as a VRAM region of its own
 * (AMDGPU_RESV_ILLUMOS_CON), so its kernel BO starts exactly at the
 * framebuffer; at attach this file wraps that BO as a DRM framebuffer
 * (patch 0018: amdgpu_illumos_gem_fb_init()) and keeps it for the
 * device's life.
 *
 * Restore (Linux: drm_fb_helper_restore_fbdev_mode_unlocked(), here
 * amdgpu_con_kms_restore()): from a work item, with every modeset lock
 * held (drm_modeset_lock_all()), a legacy set_config of a connected
 * connector's CRTC with the console framebuffer and the loader's mode
 * (from the connector's modes, else CVT), unless a client is DRM master,
 * the console is in KD_GRAPHICS or the console is shown already (by us, or
 * still by the firmware: no CRTC enabled yet).
 * Triggers (amdgpu_illumos_con.c): gfx_private's KD_TEXT, tem's
 * full-screen clear of a text VT, the DRM master's file closing and
 * lastclose (patch 0018: amdgpu_illumos_lkms_funcs).
 *
 * Panic (amdgpu_con_kms_panic()): register writes only, no locks, no
 * allocation: the console CRTC's primary surface points at the console
 * framebuffer again. Not covered: a CRTC X switched off, or a mode other
 * than the console's (the console then shows in its top-left part).
 */

#include <linux/ktime.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_encoder.h>
#include <drm/drm_file.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_modes.h>
#include <drm/drm_modeset_lock.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_print.h>
#include <drm/drm_lkms.h>

#include "amdgpu.h"
#include "amdgpu_mode.h"
#include "dce/dce_6_0_d.h"

#include "amdgpu_illumos_con.h"

/* misc/drm_kms */
extern const struct drm_illumos_lkms_funcs drm_kms_illumos_funcs;
extern struct pci_dev *lkpi_pci_dev_of(dev_info_t *dip);
/* amdgpu_display.c (patch 0018) */
extern int amdgpu_illumos_gem_fb_init(struct drm_device *dev,
    struct amdgpu_framebuffer *rfb, const struct drm_format_info *info,
    const struct drm_mode_fb_cmd2 *mode_cmd, struct drm_gem_object *obj);

/*
 * amdgpu's hook table for misc/drm_kms (patch 0018): drm_kms's, with the
 * DRM master's file close and lastclose also restoring the console.
 */
struct drm_illumos_lkms_funcs amdgpu_illumos_lkms_funcs;

static struct amdgpu_con_kms {
	bool			active;
	struct drm_device	*drm;
	struct amdgpu_device	*adev;
	struct amdgpu_framebuffer *rfb;
	uint64_t		gpu_addr;	/* the framebuffer's MC address */
	uint32_t		width, height, pitch;
	struct drm_crtc		*crtc;		/* last restored CRTC */
	struct work_struct	restore_work;
	/* the master's file whose close queued a restore (compared only) */
	struct drm_file		*closing;
} amdgpu_con;

static void
amdgpu_con_file_close(struct drm_device *dev, struct drm_file *file)
{
	bool master = READ_ONCE(file->is_master);

	drm_kms_illumos_funcs.file_close(dev, file);
	if (master && READ_ONCE(amdgpu_con.active) && amdgpu_con.drm == dev) {
		WRITE_ONCE(amdgpu_con.closing, file);
		amdgpu_con_master_gone();
	}
}

static void
amdgpu_con_lastclose(struct drm_device *dev)
{
	if (drm_kms_illumos_funcs.lastclose != NULL)
		drm_kms_illumos_funcs.lastclose(dev);
	if (READ_ONCE(amdgpu_con.active) && amdgpu_con.drm == dev)
		amdgpu_con_kms_restore();
}

/* Whether a file other than the closing one holds the DRM master. */
static bool
con_master_present(struct drm_device *drm, struct drm_file *closing)
{
	struct drm_file *file;
	bool found = false;

	mutex_lock(&drm->filelist_mutex);
	list_for_each_entry(file, &drm->filelist, lhead) {
		if (file != closing && READ_ONCE(file->is_master)) {
			found = true;
			break;
		}
	}
	mutex_unlock(&drm->filelist_mutex);
	return (found);
}

/* Whether the console framebuffer is on the display already. */
static bool
con_shown(void)
{
	struct drm_crtc *crtc = amdgpu_con.crtc;

	if (crtc == NULL)
		return (false);
	/* Atomic (DC): the committed state, not the legacy fields. */
	if (drm_drv_uses_atomic_modeset(crtc->dev))
		return (crtc->state != NULL && crtc->state->active &&
		    crtc->primary->state != NULL &&
		    crtc->primary->state->fb == &amdgpu_con.rfb->base);
	return (crtc->enabled &&
	    crtc->primary->fb == &amdgpu_con.rfb->base);
}

/*
 * Whether the display is still as the firmware left it: no CRTC enabled in
 * DRM's view. The loader's framebuffer (the console's) is on screen then,
 * and connectors may not be probed yet: tem's first full-screen clear at
 * boot comes before any DRM client, and a set_config would only fail.
 */
static bool
con_firmware_scanout(struct drm_device *drm)
{
	struct drm_crtc *c;

	/*
	 * Display Core (Polaris and later) programs the display engine at
	 * hw_init and blanks what the firmware showed: the loader's
	 * framebuffer is not on screen any more, whatever DRM's CRTC state
	 * says (MacBookPro14,3, 2026-11-17: black from DC's init on).
	 */
	if (amdgpu_con.adev->dc_enabled)
		return (false);

	drm_for_each_crtc(c, drm) {
		if (c->enabled)
			return (false);
	}
	return (true);
}

/*
 * A connected connector, its CRTC, and the console's mode on it.
 * ctx: the acquire context the modeset locks are held with (atomic
 * drivers, DC); NULL for the legacy path under drm_modeset_lock_all().
 * With ctx, -EDEADLK is returned for the caller's backoff.
 */
static int
con_set(struct drm_device *drm, struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_connector_list_iter iter;
	struct drm_connector *conn, *found = NULL;
	struct drm_encoder *enc = NULL;
	struct drm_crtc *crtc = NULL, *c;
	struct drm_display_mode *mode = NULL, *m, *cvt = NULL;
	struct drm_mode_set set;
	int ret;

	drm_connector_list_iter_begin(drm, &iter);
	drm_for_each_connector_iter(conn, &iter) {
		if (conn->status == connector_status_connected) {
			found = conn;
			drm_connector_get(found);
			break;
		}
	}
	drm_connector_list_iter_end(&iter);
	if (found == NULL)
		return (-ENODEV);

	if (found->encoder != NULL)
		enc = found->encoder;
	else
		drm_connector_for_each_possible_encoder(found, enc)
			break;
	if (enc != NULL && enc->crtc != NULL)
		crtc = enc->crtc;
	else if (enc != NULL) {
		drm_for_each_crtc(c, drm) {
			if (enc->possible_crtcs & drm_crtc_mask(c)) {
				crtc = c;
				break;
			}
		}
	}
	if (crtc == NULL) {
		drm_connector_put(found);
		return (-ENODEV);
	}

	list_for_each_entry(m, &found->modes, head) {
		if (m->hdisplay == amdgpu_con.width &&
		    m->vdisplay == amdgpu_con.height) {
			mode = m;
			if (m->type & DRM_MODE_TYPE_PREFERRED)
				break;
		}
	}
	/*
	 * No mode of the console's size (a panel whose firmware mode is
	 * scaled, e.g. 3360x2100 on a 2880x1800 eDP): the largest mode that
	 * fits in the framebuffer, preferred first, which shows the
	 * console's top-left part.
	 */
	if (mode == NULL) {
		list_for_each_entry(m, &found->modes, head) {
			if (m->hdisplay > amdgpu_con.width ||
			    m->vdisplay > amdgpu_con.height)
				continue;
			if (mode == NULL || (m->type & DRM_MODE_TYPE_PREFERRED) ||
			    (!(mode->type & DRM_MODE_TYPE_PREFERRED) &&
			    m->hdisplay * m->vdisplay >
			    mode->hdisplay * mode->vdisplay))
				mode = m;
		}
	}
	if (mode == NULL) {
		cvt = drm_cvt_mode(drm, amdgpu_con.width, amdgpu_con.height,
		    60, false, false, false);
		if (cvt == NULL) {
			drm_connector_put(found);
			return (-ENOMEM);
		}
		mode = cvt;
	}

	memset(&set, 0, sizeof (set));
	set.crtc = crtc;
	set.fb = &amdgpu_con.rfb->base;
	set.mode = mode;
	set.x = 0;
	set.y = 0;
	set.connectors = &found;
	set.num_connectors = 1;
	/*
	 * Atomic (DC): drm_mode_set_config_internal() is legacy only (it
	 * WARNs on atomic drivers and passes no acquire context, so
	 * drm_atomic_helper_set_config() took the CRTC lock without one
	 * while this thread held it with drm_modeset_lock_all()'s: the
	 * restore work deadlocked and X's GETCONNECTOR waited on
	 * mode_config.mutex for ever, MacBookPro14,3 2026-11-18). The
	 * helper with our context instead.
	 */
	if (ctx != NULL)
		ret = drm_atomic_helper_set_config(&set, ctx);
	else
		ret = drm_mode_set_config_internal(&set);
	if (ret == 0)
		WRITE_ONCE(amdgpu_con.crtc, crtc);
	if (cvt != NULL)
		drm_mode_destroy(drm, cvt);
	drm_connector_put(found);
	return (ret);
}

/*
 * Fill the connectors' mode lists (a probe, as a client's GETCONNECTOR
 * does), with mode_config.mutex, before the modeset locks. DC fills them
 * only on a probe; legacy DCE6 had them from its own init.
 */
static void
con_probe(struct drm_device *drm)
{
	struct drm_connector_list_iter iter;
	struct drm_connector *conn;

	mutex_lock(&drm->mode_config.mutex);
	drm_connector_list_iter_begin(drm, &iter);
	drm_for_each_connector_iter(conn, &iter) {
		if (list_empty(&conn->modes) && conn->funcs->fill_modes != NULL)
			(void) conn->funcs->fill_modes(conn,
			    drm->mode_config.max_width,
			    drm->mode_config.max_height);
	}
	drm_connector_list_iter_end(&iter);
	mutex_unlock(&drm->mode_config.mutex);
}

/*
 * The restore on an atomic driver (DC), locked as Linux's
 * drm_client_modeset_commit_atomic() does: every modeset lock with an
 * acquire context, backoff and retry on -EDEADLK (a DRM client, e.g. X
 * starting, contending for the same locks), and the commit with that
 * context. The connectors are probed first, with mode_config.mutex only
 * (DC fills their mode lists on a probe).
 */
static void
con_restore_atomic(struct drm_device *drm, struct drm_file *closing)
{
	struct drm_modeset_acquire_ctx ctx;
	struct drm_crtc *crtc;
	struct drm_plane_state *ps;
	uint64_t scan = 0;
	int w = 0, h = 0, pipe = -1;
	bool tried;
	int ret = 0;

	con_probe(drm);
	DRM_MODESET_LOCK_ALL_BEGIN(drm, ctx, 0, ret);
	tried = false;
	if (amdgpu_con_text_mode() && !con_master_present(drm, closing) &&
	    !con_shown()) {
		tried = true;
		ret = con_set(drm, &ctx);
		/* What DC scans out now (prepare_fb set afb->address). */
		crtc = amdgpu_con.crtc;
		if (ret == 0 && crtc != NULL && crtc->state != NULL &&
		    (ps = crtc->primary->state) != NULL && ps->fb != NULL) {
			scan = to_amdgpu_framebuffer(ps->fb)->address;
			w = crtc->state->mode.hdisplay;
			h = crtc->state->mode.vdisplay;
			pipe = drm_crtc_index(crtc);
		}
	}
	DRM_MODESET_LOCK_ALL_END(drm, ctx, ret);
	if (tried)
		drm_info(drm, "console: restored at %llu ms (%s, %d): crtc %d "
		    "%dx%d of %ux%u, scanout 0x%llx, console 0x%llx\n",
		    (unsigned long long)(ktime_get_ns() / 1000000),
		    closing != NULL ? "master closed" : "text mode", ret,
		    pipe, w, h, amdgpu_con.width, amdgpu_con.height,
		    (unsigned long long)scan,
		    (unsigned long long)amdgpu_con.gpu_addr);
	if (ret != 0)
		drm_err(drm, "console: restore failed (%d)\n", ret);
	/*
	 * DC's init left the framebuffer's VRAM without the console's
	 * text: tem's content back from gfx_private's shadow.
	 */
	else if (tried)
		amdgpu_con_repaint();
}

static void
con_restore_work(struct work_struct *work)
{
	struct drm_device *drm = amdgpu_con.drm;
	struct drm_file *closing = xchg(&amdgpu_con.closing, NULL);
	int ret = 0;

	if (!READ_ONCE(amdgpu_con.active) || !amdgpu_con_text_mode())
		return;
	if (amdgpu_con.adev->dc_enabled) {
		con_restore_atomic(drm, closing);
		return;
	}
	drm_modeset_lock_all(drm);
	/*
	 * With every modeset lock held: X's own SETCRTC waits. Nothing to do
	 * while a client is master (X in front of its VT), the console is in
	 * KD_GRAPHICS, or the console is shown (by us, or still by the
	 * firmware).
	 */
	if (amdgpu_con_text_mode() && !con_master_present(drm, closing) &&
	    !con_shown() && !con_firmware_scanout(drm)) {
		ret = con_set(drm, NULL);
		/* Measurement: a console modeset, in ms since boot. */
		drm_info(drm, "console: restored at %llu ms (%s, %d)\n",
		    (unsigned long long)(ktime_get_ns() / 1000000),
		    closing != NULL ? "master closed" : "text mode", ret);
	}
	drm_modeset_unlock_all(drm);
	if (ret != 0)
		drm_err(drm, "console: restore failed (%d)\n", ret);
}

int
amdgpu_con_kms_takeover(dev_info_t *dip, const struct amdgpu_con_fb *want)
{
	struct pci_dev *pdev = lkpi_pci_dev_of(dip);
	struct drm_device *drm;
	struct amdgpu_device *adev;
	struct amdgpu_bo *bo;
	struct amdgpu_framebuffer *rfb;
	struct drm_mode_fb_cmd2 cmd;
	int ret;

	if (pdev == NULL || (drm = pci_get_drvdata(pdev)) == NULL)
		return (-ENODEV);
	adev = drm_to_adev(drm);
	/*
	 * No display block (Polaris without Display Core, a headless GPU):
	 * DRM's mode config was never set up, so there is no framebuffer to
	 * build and no CRTC to restore. The console stays on the loader's
	 * framebuffer through gfx_private, which the firmware keeps scanning
	 * out (amdgpu does not touch the display engine then).
	 */
	if (!adev->mode_info.mode_config_initialized)
		return (-ENODEV);
	bo = adev->mman.resv_region[AMDGPU_RESV_ILLUMOS_CON].bo;
	if (bo == NULL || want->bpp != 4 ||
	    adev->mman.resv_region[AMDGPU_RESV_ILLUMOS_CON].size <
	    (uint64_t)want->pitch * want->height)
		return (-ENOENT);

	rfb = kzalloc(sizeof (*rfb), GFP_KERNEL);
	if (rfb == NULL)
		return (-ENOMEM);
	memset(&cmd, 0, sizeof (cmd));
	cmd.width = want->width;
	cmd.height = want->height;
	cmd.pixel_format = DRM_FORMAT_XRGB8888;
	cmd.pitches[0] = want->pitch;
	ret = amdgpu_illumos_gem_fb_init(drm, rfb,
	    drm_format_info(DRM_FORMAT_XRGB8888), &cmd, &bo->tbo.base);
	if (ret != 0) {
		kfree(rfb);
		return (ret);
	}
	/* The framebuffer's reference is never dropped: the console stays. */
	amdgpu_con.drm = drm;
	amdgpu_con.adev = adev;
	amdgpu_con.rfb = rfb;
	amdgpu_con.gpu_addr = amdgpu_bo_gpu_offset(bo);
	amdgpu_con.width = want->width;
	amdgpu_con.height = want->height;
	amdgpu_con.pitch = want->pitch;
	WRITE_ONCE(amdgpu_con.active, true);
	/*
	 * DC blanked the loader's framebuffer at hw_init: put the console on
	 * a connected connector now, as Linux's fbdev does at attach.
	 */
	if (adev->dc_enabled)
		amdgpu_con_kms_restore();
	return (0);
}

void
amdgpu_con_kms_restore(void)
{
	if (READ_ONCE(amdgpu_con.active))
		(void) schedule_work(&amdgpu_con.restore_work);
}

int
amdgpu_con_kms_panic(uint32_t *surfp)
{
	struct amdgpu_device *adev = amdgpu_con.adev;
	struct drm_crtc *crtc = READ_ONCE(amdgpu_con.crtc);
	u32 off;

	*surfp = 0;
	if (!READ_ONCE(amdgpu_con.active) || crtc == NULL ||
	    adev->asic_type > CHIP_HAINAN)
		return (0);
	/* DCE6 only: the surface and pitch registers of dce_v6_0.c. */
	off = to_amdgpu_crtc(crtc)->crtc_offset;
	WREG32(mmGRPH_PITCH + off, amdgpu_con.pitch / 4);
	WREG32(mmGRPH_PRIMARY_SURFACE_ADDRESS_HIGH + off,
	    upper_32_bits(amdgpu_con.gpu_addr));
	*surfp = lower_32_bits(amdgpu_con.gpu_addr);
	WREG32(mmGRPH_PRIMARY_SURFACE_ADDRESS + off, *surfp);
	return (1);
}

void
amdgpu_con_kms_init(void)
{
	amdgpu_illumos_lkms_funcs = drm_kms_illumos_funcs;
	amdgpu_illumos_lkms_funcs.file_close = amdgpu_con_file_close;
	amdgpu_illumos_lkms_funcs.lastclose = amdgpu_con_lastclose;
	INIT_WORK(&amdgpu_con.restore_work, con_restore_work);
}

void
amdgpu_con_kms_fini(void)
{
	/* Reached only when no device is the console (it refuses detach). */
	(void) cancel_work_sync(&amdgpu_con.restore_work);
}
