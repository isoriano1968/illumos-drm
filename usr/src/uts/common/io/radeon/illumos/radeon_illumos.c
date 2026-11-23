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
#include <drm/drm_device.h>

#include "radeon.h"
#include "radeon_asic.h"

extern int lkpi_module_init(void);
extern void lkpi_module_exit(void);

static struct pci_driver *radeon_pci_driver;

static int
radeon_illumos_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int ret;

	if (radeon_pci_driver == NULL)
		return (DDI_FAILURE);
	ret = lkpi_pci_attach(dip, cmd, radeon_pci_driver);
#if defined(__sparc)
	/*
	 * sun4u (Ultra 45, XVR-300): a failed attach of the display node is
	 * followed by ddi_uninitchild(), whose pcie_disable_errors() reads
	 * the card's config space and ends in a bus error (BERR panic,
	 * 2026-11-23). Until that is understood, the node stays attached
	 * without a working device when radeon's probe fails; radeon has
	 * no PCI remove anyway, so it is never detached.
	 */
	if (cmd == DDI_ATTACH && ret != DDI_SUCCESS) {
		cmn_err(CE_WARN, "radeon%d: probe failed; the node stays "
		    "attached without a device (SPARC: no teardown)",
		    ddi_get_instance(dip));
		ret = DDI_SUCCESS;
	}
#endif
	return (ret);
}

static int
radeon_illumos_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	if (radeon_pci_driver == NULL)
		return (DDI_FAILURE);
	return (lkpi_pci_detach(dip, cmd, radeon_pci_driver));
}

/*
 * Stops the GPU's own DMA before the system goes down: reset(9E), which
 * sun4u's reboot calls for every leaf driver (reset_leaves()), and
 * quiesce(9E) (fast reboot, panic).
 *
 * OBP does not POST the card on a warm reboot, so the command processor
 * and its ring read pointer and scratch writeback stay armed with the
 * previous boot's addresses. When the next boot's radeon resets the GPU,
 * r300_asic_reset() stops the CP with RB_RPTR_WR_ENA, the GPU writes the
 * read pointer back to that stale address, and px reports
 * ereport.io.fire.dmc.trn_oor (DMA outside the IOMMU range) at every
 * attach after a warm reboot, never after a power-on (U45, XVR-300,
 * 2026-11-23). Linux stops the GPU at shutdown on Open Firmware machines
 * for the same reason (radeon_pci_shutdown(), CONFIG_PPC64).
 *
 * Neither entry point may block or take locks the system may hold:
 * registers and config space only (radeon_suspend_kms(), which Linux
 * uses, sleeps).
 */
static void
radeon_illumos_stop_dma(dev_info_t *dip)
{
	struct pci_dev *pdev = ddi_get_driver_private(dip);
	struct drm_device *ddev;
	struct radeon_device *rdev;

	if (pdev == NULL || (ddev = pci_get_drvdata(pdev)) == NULL ||
	    (rdev = ddev->dev_private) == NULL || rdev->rmmio == NULL)
		return;
	if (rdev->family < CHIP_R600) {
		/* No interrupts, no ring or scratch writeback, CP halted. */
		WREG32(RADEON_GEN_INT_CNTL, 0);
		WREG32(RADEON_CP_RB_CNTL,
		    RREG32(RADEON_CP_RB_CNTL) | RADEON_RB_NO_UPDATE);
		WREG32(RADEON_SCRATCH_UMSK, 0);
		WREG32(RADEON_CP_CSQ_CNTL, 0);
		WREG32(RADEON_CP_CSQ_MODE, 0);
		/* The GPU's bus master switch, then PCI bus mastering. */
		r100_bm_disable(rdev);
		return;
	}
	/*
	 * R600 and later halt their CP with family-specific registers; to be
	 * added with the first such card on SPARC (FirePro V4900). Until
	 * then no DMA leaves the card at least.
	 */
	pci_clear_master(pdev);
}

static int
radeon_illumos_reset(dev_info_t *dip, ddi_reset_cmd_t cmd)
{
	(void) cmd;
	radeon_illumos_stop_dma(dip);
	return (DDI_SUCCESS);
}

static int
radeon_illumos_quiesce(dev_info_t *dip)
{
	radeon_illumos_stop_dma(dip);
	return (DDI_SUCCESS);
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
	.devo_reset = radeon_illumos_reset,
	.devo_cb_ops = &radeon_cb_ops,
	.devo_bus_ops = NULL,
	.devo_power = NULL,
	.devo_quiesce = radeon_illumos_quiesce,
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
