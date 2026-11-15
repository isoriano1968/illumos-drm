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

	return (crtc != NULL && crtc->enabled &&
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

	drm_for_each_crtc(c, drm) {
		if (c->enabled)
			return (false);
	}
	return (true);
}

/* A connected connector, its CRTC, and the console's mode on it. */
static int
con_set(struct drm_device *drm)
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
	ret = drm_mode_set_config_internal(&set);
	if (ret == 0)
		WRITE_ONCE(amdgpu_con.crtc, crtc);
	if (cvt != NULL)
		drm_mode_destroy(drm, cvt);
	drm_connector_put(found);
	return (ret);
}

static void
con_restore_work(struct work_struct *work)
{
	struct drm_device *drm = amdgpu_con.drm;
	struct drm_file *closing = xchg(&amdgpu_con.closing, NULL);
	int ret = 0;

	if (!READ_ONCE(amdgpu_con.active) || !amdgpu_con_text_mode())
		return;
	drm_modeset_lock_all(drm);
	/*
	 * With every modeset lock held: X's own SETCRTC waits. Nothing to do
	 * while a client is master (X in front of its VT), the console is in
	 * KD_GRAPHICS, or the console is shown (by us, or still by the
	 * firmware).
	 */
	if (amdgpu_con_text_mode() && !con_master_present(drm, closing) &&
	    !con_shown() && !con_firmware_scanout(drm)) {
		ret = con_set(drm);
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
