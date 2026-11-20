/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * The illumos console on amdgpu's display, the illumos side, after
 * i915n's (drivers/i915/illumos/i915_illumos_con.c, same design; to be
 * shared once both run): consconfig() opens the first minor node of the
 * console display device, tem drives it through VIS_DEVINIT, VIS_CONS*
 * and KDSETMODE, and misc/gfx_private implements that for the loader's
 * framebuffer (fb_info), as vgatext uses it without a DRM driver.
 *
 *	- at attach, before the DRM device and its minors exist,
 *	  gfxp_fb_attach() on amdgpu's node and the minor node
 *	  "gfx<instance>" (DDI_NT_DISPLAY), the console and /dev/fb, when the
 *	  loader left a 32-bit RGB framebuffer (UEFI GOP) inside this GPU's
 *	  VRAM BAR; amdgpu keeps that VRAM range reserved
 *	  (amdgpu_illumos_con_fb_off/size, read by
 *	  amdgpu_gmc_get_vbios_allocations(), patch 0017), and its display
 *	  engine keeps scanning it out until a DRM client sets a mode;
 *	- the console minor's open, close, ioctl, devmap and segmap go to
 *	  gfx_private, every other minor number to the DRM core;
 *	- gfx_private's mode hook records KD_TEXT/KD_GRAPHICS; a KD_TEXT, a
 *	  text VT's full clear or the DRM master's file closing calls
 *	  amdgpu_con_kms_restore() (amdgpu_illumos_con_kms.c), which puts the
 *	  console framebuffer back on the display;
 *	- a CB_CL_PANIC callback points the console CRTC at it again and
 *	  switches gfx_private to KD_TEXT;
 *	- a console device refuses detach.
 *
 * One console per system: the device whose VRAM BAR holds the loader
 * framebuffer (on the Mac Pro (Late 2013) the display GPU only).
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/stat.h>
#include <sys/pci.h>
#include <sys/kd.h>
#include <sys/visual_io.h>
#include <sys/gfx_private.h>
#include <drm/drm_drv.h>

#include "amdgpu_illumos_con.h"

/*
 * The leading members of the gate's i86pc fb_info_t (uts/i86pc/sys/
 * framebuffer.h; that platform header is not installed), as far as this
 * file reads them. The loader's framebuffer description, filled before
 * the kernel starts and only read afterwards.
 */
struct amdgpu_boot_fb {
	int		fb_type;	/* fb_type_t */
	uint64_t	paddr;
	uint8_t		*fb;
	uint8_t		*shadow_fb;
	uint64_t	fb_size;
	uint32_t	pitch;
	uint8_t		bpp;		/* bytes per pixel */
	uint8_t		depth;		/* bits per pixel */
	uint8_t		fg_color;
	uint8_t		bg_color;
	struct { uint8_t pos, size; } red, green, blue;	/* rgb_info_t */
	struct { uint16_t x, y; } screen;
};
#define	AMDGPU_FB_TYPE_RGB	3	/* FB_TYPE_RGB */

extern struct amdgpu_boot_fb fb_info;	/* unix */

/* sys/callb.h does not mix with the LinuxKPI prologue. */
typedef void *amdgpu_callb_id_t;
extern amdgpu_callb_id_t callb_add(boolean_t (*)(void *, int), void *, int,
    char *);
#define	AMDGPU_CB_CL_PANIC	5	/* CB_CL_PANIC */

/* The console minor: node minors 256 + instance, unused by the DRM core. */
#define	AMDGPU_CON_MINOR_BASE	256
#define	AMDGPU_CON_MINOR_MAX	64

/*
 * The loader framebuffer's VRAM offset and size while the console's
 * device attaches, 0 otherwise: amdgpu_gmc_get_vbios_allocations()
 * reserves exactly this range (patch 0017, AMDGPU_RESV_ILLUMOS_CON), so
 * that nothing is allocated where gfx_private draws and its kernel BO
 * starts at the framebuffer (the console restore scans it out).
 */
uint64_t amdgpu_illumos_con_fb_off;
uint64_t amdgpu_illumos_con_fb_size;

static struct {
	dev_info_t		*dip;		/* the node with the console minor */
	gfxp_fb_softc_ptr_t	gfxp;
	boolean_t		console;	/* taken over: tem may use it */
	amdgpu_callb_id_t	panic_cb;
	volatile int		kd_mode;	/* KD_TEXT, KD_GRAPHICS */
	volatile boolean_t	devinit;	/* tem's VIS_DEVINIT succeeded */
	char			name[16];
} amdgpu_con_st = { .kd_mode = KD_TEXT };

/*
 * What the panic callback did, for the crash dump: amdgpu has no CTF, so
 * read it as "amdgpu_con_panic_log/4U" with mdb (runs, the KD mode found,
 * whether the plane was written, the surface address written).
 */
struct {
	uint32_t	runs;
	uint32_t	kd_mode;
	uint32_t	flipped;
	uint32_t	surf;
} amdgpu_con_panic_log;

static boolean_t
con_minor(dev_t dev)
{
	minor_t m = getminor(dev);

	return (m >= AMDGPU_CON_MINOR_BASE &&
	    m < AMDGPU_CON_MINOR_BASE + AMDGPU_CON_MINOR_MAX &&
	    amdgpu_con_st.dip != NULL &&
	    m - AMDGPU_CON_MINOR_BASE == (minor_t)ddi_get_instance(
	    amdgpu_con_st.dip));
}

static dev_t
con_dev(void)
{
	return (makedevice(ddi_driver_major(amdgpu_con_st.dip),
	    AMDGPU_CON_MINOR_BASE + ddi_get_instance(amdgpu_con_st.dip)));
}

/*
 * The console's KD mode. gfx_private's own (KDGETMODE) is the authority:
 * KD_RESETTEXT and this driver's own KDSETMODE change it without always
 * calling the mode hook, so the hook's copy (kd_mode) is only used where
 * gfx_private cannot be asked (panic) or need not be (the VIS_CONSCLEAR
 * filter in the ioctl path reads it outside gfx_private's lock, after the
 * ioctl returned: a stale value only affects whether a restore is
 * queued; the work decides with the fresh mode). The hook is the only
 * writer of kd_mode: a store here, outside gfx_private's lock, could
 * overwrite a newer KD_GRAPHICS of X's EnterVT with the KD_TEXT read
 * before it (review round 2). Thread context, without gfx_private's lock
 * held.
 */
boolean_t
amdgpu_con_text_mode(void)
{
	int mode = amdgpu_con_st.kd_mode, rval;

	if (amdgpu_con_st.devinit && panicstr == NULL &&
	    gfxp_fb_ioctl(con_dev(), KDGETMODE, (intptr_t)&mode, FKIOCTL,
	    kcred, &rval, amdgpu_con_st.gfxp) != 0)
		mode = amdgpu_con_st.kd_mode;
	return (mode == KD_TEXT);
}

/*
 * gfx_private's KDSETMODE from this driver (not from tem or X). Only
 * after tem's VIS_DEVINIT: before it, gfx_private has no console state
 * (softc->console) and its KD_TEXT would dereference NULL.
 */
static void
con_kdsetmode(int mode)
{
	int rval;

	if (!amdgpu_con_st.devinit)
		return;
	(void) gfxp_fb_ioctl(con_dev(), KDSETMODE, mode, FKIOCTL, kcred,
	    &rval, amdgpu_con_st.gfxp);
}

/*
 * Repaint the console framebuffer from gfx_private's shadow: its
 * KDSETMODE to KD_TEXT copies the shadow to the framebuffer, but only on
 * a change of mode, hence KD_GRAPHICS first (the hook records both; the
 * KD_TEXT one queues a restore, which finds the console shown). For
 * Display Core: the framebuffer's VRAM no longer holds what tem drew
 * before amdgpu's init (MacBookPro14,3: black after the restore, 2026-11-18).
 * Only in KD_TEXT, so never over X.
 */
void
amdgpu_con_repaint(void)
{
	if (!amdgpu_con_st.console || !amdgpu_con_text_mode())
		return;
	con_kdsetmode(KD_GRAPHICS);
	con_kdsetmode(KD_TEXT);
}

/* gfx_private's mode hook: called under its lock, from tem or wc. */
static int
amdgpu_con_setmode(int mode)
{
	amdgpu_con_st.kd_mode = mode;
	membar_producer();
	if (mode == KD_TEXT && panicstr == NULL)
		amdgpu_con_kms_restore();
	return (0);
}

static struct gfxp_blt_ops amdgpu_con_ops = {
	.blt = NULL,
	.copy = NULL,
	.clear = NULL,
	.setmode = amdgpu_con_setmode
};

/*
 * The DRM master's file closed (X exited or died): the console takes the
 * display back. A killed X leaves gfx_private in KD_GRAPHICS (illumos' VT
 * driver does not reset the mode of a VT whose process died): switch it
 * to KD_TEXT, which repaints the console framebuffer from gfx_private's
 * shadow. The restore is queued directly, whatever the hook saw.
 */
void
amdgpu_con_master_gone(void)
{
	if (!amdgpu_con_st.console)
		return;
	if (!amdgpu_con_text_mode())
		con_kdsetmode(KD_TEXT);
	amdgpu_con_kms_restore();
}

static boolean_t
amdgpu_con_panic(void *arg, int code)
{
	_NOTE(ARGUNUSED(arg, code));
	if (!amdgpu_con_st.console)
		return (B_TRUE);
	/*
	 * Always: the console may be in KD_TEXT while X scans out its own
	 * framebuffer (tem has X's VT in text mode once X acknowledged a VT
	 * switch), and rewriting the plane registers with what they hold is
	 * harmless. Then gfx_private to KD_TEXT, so that tem's polled panic
	 * output reaches the console framebuffer; panicstr is set, so
	 * gfx_private's lock is bypassed (mutex_vector_enter()). Always, not
	 * only when kd_mode says KD_GRAPHICS: kd_mode is the hook's copy and
	 * gfx_private's KDSETMODE of the mode it is in already does nothing.
	 */
	amdgpu_con_panic_log.runs++;
	amdgpu_con_panic_log.kd_mode = amdgpu_con_st.kd_mode;
	amdgpu_con_panic_log.flipped =
	    amdgpu_con_kms_panic(&amdgpu_con_panic_log.surf);
	con_kdsetmode(KD_TEXT);
	return (B_TRUE);
}

static boolean_t
con_wanted(dev_info_t *dip)
{
	int vendor, inst, *data;
	uint_t i, count;
	boolean_t match = B_FALSE;
	pci_regspec_t *regs;
	uint64_t fb_off = 0;

	if (amdgpu_con_st.dip != NULL)
		return (B_FALSE);
	inst = ddi_get_instance(dip);
	if (inst < 0 || inst >= AMDGPU_CON_MINOR_MAX)
		return (B_FALSE);
	vendor = ddi_prop_get_int(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "vendor-id", -1);
	if (vendor != 0x1002)
		return (B_FALSE);
	/* BAR0 is the VRAM aperture; only the GPU whose VRAM holds the
	 * loader framebuffer gets the console. */
	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "assigned-addresses", &data, &count) != DDI_PROP_SUCCESS)
		return (B_FALSE);
	regs = (pci_regspec_t *)(void *)data;
	for (i = 0; count % 5 == 0 && i < count / 5; i++) {
		uint64_t base, size;
		if (PCI_REG_REG_G(regs[i].pci_phys_hi) != PCI_CONF_BASE0)
			continue;
		base = ((uint64_t)regs[i].pci_phys_mid << 32) | regs[i].pci_phys_low;
		size = ((uint64_t)regs[i].pci_size_hi << 32) | regs[i].pci_size_low;
		if (fb_info.paddr >= base && fb_info.paddr - base < size &&
		    fb_info.fb_size && fb_info.fb_size <= size - (fb_info.paddr - base)) {
			match = B_TRUE;
			fb_off = fb_info.paddr - base;
		}
	}
	ddi_prop_free(data);
	if (!match)
		return (B_FALSE);
	/* A 32-bit RGB framebuffer from the loader (not VGA text). */
	if (fb_info.fb_type != AMDGPU_FB_TYPE_RGB || fb_info.paddr == 0 ||
	    fb_info.bpp != 4 || fb_info.red.pos != 16 ||
	    fb_info.green.pos != 8 || fb_info.blue.pos != 0) {
		(dev_err)(dip, CE_NOTE, "!console: the loader's framebuffer "
		    "(type %d, %u bytes per pixel) is not one amdgpu can keep; "
		    "no console on this device", fb_info.fb_type, fb_info.bpp);
		return (B_FALSE);
	}
	amdgpu_illumos_con_fb_off = fb_off;
	amdgpu_illumos_con_fb_size = fb_info.fb_size;
	return (B_TRUE);
}

int
amdgpu_con_attach_early(dev_info_t *dip)
{
	gfxp_fb_softc_ptr_t gfxp;
	int inst = ddi_get_instance(dip);

	if (!con_wanted(dip))
		return (DDI_SUCCESS);
	gfxp = gfxp_fb_softc_alloc();
	if (gfxp_fb_attach(dip, DDI_ATTACH, gfxp) != DDI_SUCCESS) {
		gfxp_fb_softc_free(gfxp);
		(dev_err)(dip, CE_WARN, "console: gfx_private refused the "
		    "framebuffer; no console on this device");
		return (DDI_SUCCESS);
	}
	(void) snprintf(amdgpu_con_st.name, sizeof (amdgpu_con_st.name),
	    "gfx%d", inst);
	if (ddi_create_minor_node(dip, amdgpu_con_st.name, S_IFCHR,
	    AMDGPU_CON_MINOR_BASE + inst, DDI_NT_DISPLAY, 0) != DDI_SUCCESS) {
		(void) gfxp_fb_detach(dip, DDI_DETACH, gfxp);
		gfxp_fb_softc_free(gfxp);
		(dev_err)(dip, CE_WARN, "console: cannot create minor node %s",
		    amdgpu_con_st.name);
		return (DDI_SUCCESS);
	}
	gfxp_bm_register_fbops(gfxp, &amdgpu_con_ops);
	amdgpu_con_st.gfxp = gfxp;
	amdgpu_con_st.dip = dip;
	return (DDI_SUCCESS);
}

void
amdgpu_con_attach_undo(dev_info_t *dip)
{
	if (amdgpu_con_st.dip != dip || amdgpu_con_st.console)
		return;
	ddi_remove_minor_node(dip, amdgpu_con_st.name);
	(void) gfxp_fb_detach(dip, DDI_DETACH, amdgpu_con_st.gfxp);
	gfxp_fb_softc_free(amdgpu_con_st.gfxp);
	amdgpu_con_st.gfxp = NULL;
	amdgpu_con_st.dip = NULL;
}

boolean_t
amdgpu_con_boot_fb(dev_info_t *dip, struct amdgpu_con_fb *fb)
{
	if (amdgpu_con_st.dip != dip)
		return (B_FALSE);
	fb->paddr = fb_info.paddr;
	fb->size = fb_info.fb_size;
	fb->pitch = fb_info.pitch;
	fb->width = fb_info.screen.x;
	fb->height = fb_info.screen.y;
	fb->bpp = fb_info.bpp;
	return (B_TRUE);
}

int
amdgpu_con_attach_late(dev_info_t *dip)
{
	struct amdgpu_con_fb fb;

	amdgpu_illumos_con_fb_off = 0;
	amdgpu_illumos_con_fb_size = 0;
	if (amdgpu_con_st.dip != dip)
		return (DDI_SUCCESS);
	fb.paddr = fb_info.paddr;
	fb.size = fb_info.fb_size;
	fb.pitch = fb_info.pitch;
	fb.width = fb_info.screen.x;
	fb.height = fb_info.screen.y;
	fb.bpp = fb_info.bpp;
	/*
	 * The display still scans out the loader framebuffer (amdgpu sets no
	 * mode before a DRM client does) and its VRAM is reserved (patch
	 * 0017): tem may use it. The KMS side wraps it as a DRM framebuffer
	 * to put the console back after X; without that the console still
	 * works until X starts.
	 */
	if (amdgpu_con_kms_takeover(dip, &fb) != 0)
		(dev_err)(dip, CE_WARN, "console: the loader framebuffer "
		    "cannot be restored after X (no KMS takeover)");
	amdgpu_con_st.console = B_TRUE;
	amdgpu_con_st.panic_cb = callb_add(amdgpu_con_panic, NULL,
	    AMDGPU_CB_CL_PANIC, "amdgpu console");
	(dev_err)(dip, CE_NOTE, "!console: the loader framebuffer "
	    "(%ux%u, %u bytes per line) is the illumos console",
	    fb_info.screen.x, fb_info.screen.y, fb_info.pitch);
	return (DDI_SUCCESS);
}

boolean_t
amdgpu_con_is_console(dev_info_t *dip)
{
	return (amdgpu_con_st.dip == dip && amdgpu_con_st.console);
}

/* --- cb_ops: the console minor to gfx_private, the rest to the core --- */

int
amdgpu_con_open(dev_t *devp, int flag, int otyp, cred_t *credp)
{
	if (con_minor(*devp))
		return (gfxp_fb_open(devp, flag, otyp, credp,
		    amdgpu_con_st.gfxp));
	return (drm_open(devp, flag, otyp, credp));
}

int
amdgpu_con_close(dev_t dev, int flag, int otyp, cred_t *credp)
{
	if (con_minor(dev))
		return (gfxp_fb_close(dev, flag, otyp, credp,
		    amdgpu_con_st.gfxp));
	return (drm_close(dev, flag, otyp, credp));
}

int
amdgpu_con_read(dev_t dev, struct uio *uiop, cred_t *credp)
{
	if (con_minor(dev))
		return (ENXIO);
	return (drm_read(dev, uiop, credp));
}

int
amdgpu_con_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp,
    int *rvalp)
{
	int ret;

	if (!con_minor(dev))
		return (drm_ioctl(dev, cmd, arg, mode, credp, rvalp));
	switch (cmd) {
	case VIS_CONSDISPLAY:
	case VIS_CONSCOPY:
	case VIS_CONSCURSOR:
	case VIS_CONSCLEAR:
	case VIS_DEVFINI:
		/*
		 * tem's commands, from the kernel only: gfx_private trusts
		 * their coordinates and data pointer (it checks FKIOCTL only
		 * for VIS_DEVINIT).
		 */
		if ((mode & FKIOCTL) == 0)
			return (EPERM);
		break;
	default:
		break;
	}
	/* tem uses VIS_CONSCLEAR's *rvalp, which gfx_private leaves. */
	*rvalp = 0;
	ret = gfxp_fb_ioctl(dev, cmd, arg, mode, credp, rvalp,
	    amdgpu_con_st.gfxp);
	if (cmd == VIS_DEVINIT && ret == 0)
		amdgpu_con_st.devinit = B_TRUE;
	/*
	 * tem repaints a text VT that becomes active with a full clear. A
	 * switch between two VTs that tem has in text mode (X's own VT is in
	 * text mode again once X acknowledged a switch) sends no KDSETMODE,
	 * so the clear is the signal that a text VT is in front: queue the
	 * restore, which does nothing while a DRM client is master or the
	 * console is shown already.
	 */
	if (cmd == VIS_CONSCLEAR && ret == 0 && amdgpu_con_st.console &&
	    amdgpu_con_st.kd_mode == KD_TEXT && panicstr == NULL)
		amdgpu_con_kms_restore();
	return (ret);
}

int
amdgpu_con_chpoll(dev_t dev, short events, int anyyet, short *reventsp,
    struct pollhead **phpp)
{
	if (con_minor(dev))
		return (ENXIO);
	return (drm_chpoll(dev, events, anyyet, reventsp, phpp));
}

/*
 * prop_op(9E): specfs asks for "Size" to answer stat(2) and lseek(SEEK_END)
 * on a dma-buf descriptor (an anonymous minor of the DRM core); answer
 * with the buffer's size, as Linux's dma_buf_llseek does. Everything else
 * is ddi_prop_op's.
 */
int
amdgpu_con_prop_op(dev_t dev, dev_info_t *dip, ddi_prop_op_t op, int flags,
    char *name, caddr_t valuep, int *lengthp)
{
	uint64_t size;

	if (dev != DDI_DEV_T_ANY && dev != DDI_DEV_T_NONE && !con_minor(dev) &&
	    drm_dmabuf_size(dev, &size) == 0)
		return (ddi_prop_op_size(dev, dip, op, flags, name, valuep,
		    lengthp, size));
	return (ddi_prop_op(dev, dip, op, flags, name, valuep, lengthp));
}

int
amdgpu_con_devmap(dev_t dev, devmap_cookie_t dhp, offset_t off, size_t len,
    size_t *maplen, uint_t model)
{
	if (con_minor(dev))
		return (gfxp_fb_devmap(dev, dhp, off, len, maplen, model,
		    amdgpu_con_st.gfxp));
	return (drm_devmap(dev, dhp, off, len, maplen, model));
}

int
amdgpu_con_segmap(dev_t dev, off_t off, struct as *asp, caddr_t *addrp,
    off_t len, uint_t prot, uint_t maxprot, uint_t flags, cred_t *credp)
{
	if (con_minor(dev))
		return (ddi_devmap_segmap(dev, off, asp, addrp, len, prot,
		    maxprot, flags, credp));
	return (drm_segmap(dev, off, asp, addrp, len, prot, maxprot, flags,
	    credp));
}

int
amdgpu_con_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg,
    void **resultp)
{
	dev_t dev = (dev_t)(uintptr_t)arg;

	if (!con_minor(dev))
		return (drm_getinfo(dip, cmd, arg, resultp));
	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*resultp = amdgpu_con_st.dip;
		return (DDI_SUCCESS);
	case DDI_INFO_DEVT2INSTANCE:
		*resultp = (void *)(uintptr_t)(getminor(dev) -
		    AMDGPU_CON_MINOR_BASE);
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}
}
