/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The OpenIndiana SPARC desktop contributors
 *
 * The illumos module of Linux's radeon driver: the kernel module "radeon".
 *
 * _init checks the DRM facade and the LinuxKPI layer it was built
 * against, runs radeon's module_init (which registers its struct
 * pci_driver with the layer) and installs the module. attach(9E) and
 * detach(9E) are the layer's PCI glue with radeon's driver; the character
 * device is the DRM core's (DRM_CB_OPS). Modelled on illumos-drm's
 * amdgpu_illumos.c, without its console: on SPARC the console stays
 * OBP's (radeon must not take the console framebuffer at boot, see
 * ports/illumos-sparc/drm/radeon-m1/README.md).
 */

#include <linux/pci.h>
#include <linux/module.h>
#include <lkpi/version.h>
#include <drm/drm_facade.h>

extern int lkpi_module_init(void);
extern void lkpi_module_exit(void);

static struct pci_driver *radeon_pci_driver;

static int
radeon_illumos_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	if (radeon_pci_driver == NULL)
		return (DDI_FAILURE);
	return (lkpi_pci_attach(dip, cmd, radeon_pci_driver));
}

static int
radeon_illumos_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	if (radeon_pci_driver == NULL)
		return (DDI_FAILURE);
	return (lkpi_pci_detach(dip, cmd, radeon_pci_driver));
}

static struct cb_ops radeon_cb_ops = DRM_CB_OPS;

static struct dev_ops radeon_dev_ops = {
	.devo_rev = DEVO_REV,
	.devo_refcnt = 0,
	.devo_getinfo = drm_getinfo,
	.devo_identify = nulldev,
	.devo_probe = nulldev,
	.devo_attach = radeon_illumos_attach,
	.devo_detach = radeon_illumos_detach,
	.devo_reset = nodev,
	.devo_cb_ops = &radeon_cb_ops,
	.devo_bus_ops = NULL,
	.devo_power = NULL,
	/* The GPU keeps running what radeon left it doing. */
	.devo_quiesce = ddi_quiesce_not_supported,
};

static struct modldrv radeon_modldrv = {
	.drv_modops = &mod_driverops,
	.drv_linkinfo = "ATI Radeon (Linux radeon)",
	.drv_dev_ops = &radeon_dev_ops,
};

static struct modlinkage radeon_modlinkage = {
	.ml_rev = MODREV_1,
	.ml_linkage = { &radeon_modldrv, NULL },
};

int
_init(void)
{
	int ret;

	if ((ret = drm_facade_check("radeon")) != 0)
		return (ret);
	if ((ret = linuxkpi_check("radeon", LINUXKPI_ABI)) != 0)
		return (ret);
	if ((ret = lkpi_module_init()) != 0) {
		cmn_err(CE_WARN, "radeon: module init failed (%d)", ret);
		lkpi_module_quiesce(&radeon_modlinkage);
		return (EINVAL);
	}
	radeon_pci_driver = lkpi_pci_driver_lookup("radeon");
	if (radeon_pci_driver == NULL) {
		cmn_err(CE_WARN, "radeon: radeon registered no PCI driver");
		lkpi_module_exit();
		lkpi_module_quiesce(&radeon_modlinkage);
		return (EINVAL);
	}
	if ((ret = mod_install(&radeon_modlinkage)) != 0) {
		radeon_pci_driver = NULL;
		lkpi_module_exit();
		lkpi_module_quiesce(&radeon_modlinkage);
	}
	return (ret);
}

int
_fini(void)
{
	int ret;

	if ((ret = mod_remove(&radeon_modlinkage)) != 0)
		return (ret);
	radeon_pci_driver = NULL;
	lkpi_module_exit();
	/* Nothing of the layer may call into this module's text any more. */
	lkpi_module_quiesce(&radeon_modlinkage);
	return (0);
}

int
_info(struct modinfo *mi)
{
	return (mod_info(&radeon_modlinkage, mi));
}
