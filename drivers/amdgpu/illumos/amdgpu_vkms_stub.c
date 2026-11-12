/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * The virtual display IP block (amdgpu.virtual_display=, SR-IOV guests,
 * GPUs without a display engine). Linux's amdgpu_vkms.c is GPL-2.0+, so
 * it is not built; this block fails early_init, and amdgpu then runs
 * without the virtual display (the module parameter is refused).
 */
#include <linux/errno.h>
#include "amdgpu.h"
#include "amdgpu_vkms.h"

static int
amdgpu_vkms_illumos_early_init(struct amdgpu_ip_block *ip_block)
{
	dev_err(ip_block->adev->dev,
	    "virtual display is not available on illumos\n");
	return (-ENODEV);
}

static const struct amd_ip_funcs amdgpu_vkms_illumos_funcs = {
	.name = "amdgpu_vkms",
	.early_init = amdgpu_vkms_illumos_early_init,
};

const struct amdgpu_ip_block_version amdgpu_vkms_ip_block = {
	.type = AMD_IP_BLOCK_TYPE_DCE,
	.major = 1,
	.minor = 0,
	.rev = 0,
	.funcs = &amdgpu_vkms_illumos_funcs,
};
