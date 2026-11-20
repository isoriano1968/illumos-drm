/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * The illumos console on amdgpu's display (display build only): the
 * interface between i915_illumos_con.c (the illumos side: gfx_private, the
 * console minor node, KD modes, the panic callback) and
 * amdgpu_illumos_con_kms.c (the KMS side: the console framebuffer, its
 * restore commit, the panic flip). The two halves are separate files
 * because the illumos console headers and Linux's i915 headers do not mix.
 */

#ifndef _AMDGPU_ILLUMOS_CON_H
#define	_AMDGPU_ILLUMOS_CON_H

/* The loader's framebuffer, as the console (gfx_private) draws into it. */
struct amdgpu_con_fb {
	uint64_t	paddr;		/* physical address (GPU aperture) */
	uint64_t	size;		/* bytes */
	uint32_t	pitch;		/* bytes per line */
	uint32_t	width;		/* pixels */
	uint32_t	height;		/* lines */
	uint32_t	bpp;		/* bytes per pixel */
};

/* --- the illumos side (i915_illumos_con.c) --- */

/* Loader framebuffer for the device selected as the boot console. */
extern boolean_t amdgpu_con_boot_fb(dev_info_t *, struct amdgpu_con_fb *);

/* attach(9E), before the DRM device exists: the console minor first. */
extern int amdgpu_con_attach_early(dev_info_t *dip);
/* attach(9E), after i915's probe: take the console over, or undo. */
extern int amdgpu_con_attach_late(dev_info_t *dip);
extern void amdgpu_con_attach_undo(dev_info_t *dip);
/* B_TRUE while dip is the console (detach is refused then). */
extern boolean_t amdgpu_con_is_console(dev_info_t *dip);
/* The DRM master's file closed: give the display back to the console. */
extern void amdgpu_con_master_gone(void);
/* The console shows text (KD_TEXT): the KMS side may restore it. */
extern boolean_t amdgpu_con_text_mode(void);
/* Repaint the console framebuffer from gfx_private's shadow (KD_TEXT). */
extern void amdgpu_con_repaint(void);

/* cb_ops and getinfo wrappers: the console minor, else the DRM core. */
extern int amdgpu_con_open(dev_t *devp, int flag, int otyp, cred_t *credp);
extern int amdgpu_con_close(dev_t dev, int flag, int otyp, cred_t *credp);
extern int amdgpu_con_read(dev_t dev, struct uio *uiop, cred_t *credp);
extern int amdgpu_con_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
    cred_t *credp, int *rvalp);
extern int amdgpu_con_chpoll(dev_t dev, short events, int anyyet,
    short *reventsp, struct pollhead **phpp);
extern int amdgpu_con_devmap(dev_t dev, devmap_cookie_t dhp, offset_t off,
    size_t len, size_t *maplen, uint_t model);
extern int amdgpu_con_segmap(dev_t dev, off_t off, struct as *asp,
    caddr_t *addrp, off_t len, uint_t prot, uint_t maxprot, uint_t flags,
    cred_t *credp);
extern int amdgpu_con_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg,
    void **resultp);

/* --- the KMS side (amdgpu_illumos_con_kms.c) --- */

/*
 * Finds the framebuffer i915 inherited from the firmware at fb's address
 * and geometry, keeps it (a reference and a GGTT pin for the device's
 * life) and records the console's CRTC, connectors and mode. 0, or a
 * negative errno when there is none (then there is no console).
 */
extern int amdgpu_con_kms_takeover(dev_info_t *dip,
    const struct amdgpu_con_fb *fb);
/* Queues the restore of the console configuration (thread context). */
extern void amdgpu_con_kms_restore(void);
/*
 * Panic: show the console framebuffer with register writes only. Returns
 * whether the plane was written and stores the surface address it wrote.
 */
extern int amdgpu_con_kms_panic(uint32_t *surfp);
/* Module load/unload of the KMS side (the hook table, the work item). */
extern void amdgpu_con_kms_init(void);
extern void amdgpu_con_kms_fini(void);

/* the loader framebuffer's VRAM range while its device attaches (0017) */
extern uint64_t amdgpu_illumos_con_fb_off;
extern uint64_t amdgpu_illumos_con_fb_size;
extern void amdgpu_con_kms_init(void);
extern void amdgpu_con_kms_fini(void);

#endif	/* _AMDGPU_ILLUMOS_CON_H */
