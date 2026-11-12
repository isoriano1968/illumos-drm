# SPDX-License-Identifier: MIT
# Copyright 2026 The illumos-drm authors
#
# Evaluates Linux's own amdgpu Kbuild Makefiles with GNU make for the
# illumos configuration (CONFIGS below) and prints the objects of amdgpu.ko
# as TUs relative to drivers/gpu/drm/amd/amdgpu (gen-tus.sh post-processes
# them). Nothing of the Makefiles is copied; they are read in place.
#
#   gmake -s -f gen-tus.mk LINUX=<tree> print
src := $(LINUX)/drivers/gpu/drm/amd/amdgpu
# Southern Islands (GCN 1.0: Tahiti, Pitcairn, Cape Verde, Oland, Hainan)
CONFIG_DRM_AMDGPU_SI := y
# Left out for now: CIK, DC (SI display goes through dce_v6_0), KFD, ACP,
# ISP, HMM, compat ioctls, vga_switcheroo (ATPX is GPL-2.0), ACPI, procfs
# fdinfo, perf PMU.
include $(src)/Makefile
print:
	@echo $(amdgpu-y)
