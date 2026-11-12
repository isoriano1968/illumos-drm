/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * The illumos module of Linux's amdgpu driver: the kernel module "amdgpu",
 * with mode setting through misc/drm_kms (patch 0014) but no console
 * takeover yet (Southern Islands first).
 *
 * amdgpu's object carries three Linux modules: the GPU buddy allocator
 * (drivers/gpu/buddy.c), the DRM GPU scheduler (scheduler/sched_fence.c)
 * and amdgpu itself. build.sh gives the first two their own module_init
 * names (LKPI_MODULE_INIT), so _init runs them in Linux's load order
 * (buddy, scheduler, amdgpu) and _fini unwinds them in reverse.
 *
 * _init checks the DRM facade and the LinuxKPI layer it was built
 * against, runs the three module_inits (amdgpu's registers its struct
 * pci_driver with the layer) and installs the module. attach(9E) and
 * detach(9E) are the layer's PCI glue with amdgpu's driver; the character
 * device is the DRM core's (DRM_CB_OPS). Modelled on
 * drivers/i915/illumos/i915_illumos.c.
 */

#include <linux/pci.h>
#include <linux/module.h>
#include <lkpi/version.h>
#include <drm/drm_facade.h>

extern int lkpi_module_init_gpu_buddy(void);
extern void lkpi_module_exit_gpu_buddy(void);
extern int lkpi_module_init_drm_sched(void);
extern void lkpi_module_exit_drm_sched(void);
extern int lkpi_module_init(void);
extern void lkpi_module_exit(void);

static struct pci_driver *amdgpu_pci_driver;

static int
amdgpu_illumos_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	if (amdgpu_pci_driver == NULL)
		return (DDI_FAILURE);
	return (lkpi_pci_attach(dip, cmd, amdgpu_pci_driver));
}

static int
amdgpu_illumos_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	if (amdgpu_pci_driver == NULL)
		return (DDI_FAILURE);
	return (lkpi_pci_detach(dip, cmd, amdgpu_pci_driver));
}

static struct cb_ops amdgpu_cb_ops = DRM_CB_OPS;

static struct dev_ops amdgpu_dev_ops = {
	.devo_rev = DEVO_REV,
	.devo_refcnt = 0,
	.devo_getinfo = drm_getinfo,
	.devo_identify = nulldev,
	.devo_probe = nulldev,
	.devo_attach = amdgpu_illumos_attach,
	.devo_detach = amdgpu_illumos_detach,
	.devo_reset = nodev,
	.devo_cb_ops = &amdgpu_cb_ops,
	.devo_bus_ops = NULL,
	.devo_power = NULL,
	/*
	 * The GPU keeps running whatever amdgpu left it doing; fast reboot
	 * is refused rather than handing over a busy GPU.
	 */
	.devo_quiesce = ddi_quiesce_not_supported,
};

static struct modldrv amdgpu_modldrv = {
	.drv_modops = &mod_driverops,
	.drv_linkinfo = "AMD GPU (Linux amdgpu)",
	.drv_dev_ops = &amdgpu_dev_ops,
};

static struct modlinkage amdgpu_modlinkage = {
	.ml_rev = MODREV_1,
	.ml_linkage = { &amdgpu_modldrv, NULL },
};

/* The carried modules' and amdgpu's exits, newest first. */
static void
amdgpu_illumos_exit_from(int level)
{
	if (level >= 3)
		lkpi_module_exit();
	if (level >= 2)
		lkpi_module_exit_drm_sched();
	if (level >= 1)
		lkpi_module_exit_gpu_buddy();
}

int
_init(void)
{
	int ret;

	if ((ret = drm_facade_check("amdgpu")) != 0)
		return (ret);
	if ((ret = linuxkpi_check("amdgpu", LINUXKPI_ABI)) != 0)
		return (ret);
	if ((ret = lkpi_module_init_gpu_buddy()) != 0) {
		cmn_err(CE_WARN, "amdgpu: buddy allocator init failed (%d)",
		    ret);
		goto fail;
	}
	if ((ret = lkpi_module_init_drm_sched()) != 0) {
		cmn_err(CE_WARN, "amdgpu: GPU scheduler init failed (%d)",
		    ret);
		amdgpu_illumos_exit_from(1);
		goto fail;
	}
	if ((ret = lkpi_module_init()) != 0) {
		cmn_err(CE_WARN, "amdgpu: amdgpu module init failed (%d)",
		    ret);
		amdgpu_illumos_exit_from(2);
		goto fail;
	}
	amdgpu_pci_driver = lkpi_pci_driver_lookup("amdgpu");
	if (amdgpu_pci_driver == NULL) {
		cmn_err(CE_WARN, "amdgpu: amdgpu registered no PCI driver");
		amdgpu_illumos_exit_from(3);
		goto fail;
	}
	if ((ret = mod_install(&amdgpu_modlinkage)) != 0) {
		amdgpu_pci_driver = NULL;
		amdgpu_illumos_exit_from(3);
		lkpi_module_quiesce(&amdgpu_modlinkage);
		return (ret);
	}
	return (0);
fail:
	lkpi_module_quiesce(&amdgpu_modlinkage);
	return (EINVAL);
}

int
_fini(void)
{
	int ret;

	if ((ret = mod_remove(&amdgpu_modlinkage)) != 0)
		return (ret);
	amdgpu_pci_driver = NULL;
	amdgpu_illumos_exit_from(3);
	/* Nothing of the layer may call into this module's text any more. */
	lkpi_module_quiesce(&amdgpu_modlinkage);
	return (0);
}

int
_info(struct modinfo *mi)
{
	return (mod_info(&amdgpu_modlinkage, mi));
}
