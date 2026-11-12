/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * The illumos replacement of amdgpu_vkms.h (GPL-2.0+ in Linux), which
 * fetch-and-patch.sh writes over the tree's file: only the names the MIT
 * sources use (si.c, cik.c, vi.c, nv.c, soc15.c, amdgpu_discovery.c,
 * amdgpu.h). The virtual display is not provided on illumos; its IP
 * block refuses to start (illumos/amdgpu_vkms_stub.c).
 */
#ifndef _AMDGPU_VKMS_H_
#define	_AMDGPU_VKMS_H_

struct amdgpu_vkms_output;
extern const struct amdgpu_ip_block_version amdgpu_vkms_ip_block;

#endif
