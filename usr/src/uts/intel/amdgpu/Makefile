# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
all:
MODULE = amdgpu
BUILD_MODULE = amdgpu
FLAGS_DIR = $(UTSBASE)/intel/io/amdgpu
INSTALL_DIR = kernel/drv/amd64
OUT = $(INTEL)/amdgpu/obj64
include ../../Makefile.uts
include ../Makefile.files
OBJECTS = $(AMDGPU_OBJECTS)
MAPFILES = $(LKPI_MAPFILE) $(UTSBASE)/intel/io/amdgpu/private.mapfile $(UTSBASE)/common/io/drm_kms/drm_kms.mapfile
include ../../Makefile.targ
COMPILE = $(COMPILE.tu)

# Linux's default (W=0) GCC warning set for its own files
# (scripts/Makefile.warn): these are off in every Linux build. The
# illumos glue keeps them.
LINUX_WFLAGS = -Wno-maybe-uninitialized -Wno-unused-but-set-variable \
	-Wno-unused-const-variable -Wno-dangling-pointer -Wno-array-bounds

# The carried Linux modules' own entry points (<linux/module.h>); the
# illumos glue calls them before amdgpu's.
$(OUT)/obj/drivers/gpu/buddy.o := MODULE_ENTRY = \
	-DLKPI_MODULE_INIT=lkpi_module_init_gpu_buddy \
	-DLKPI_MODULE_EXIT=lkpi_module_exit_gpu_buddy
$(OUT)/obj/drivers/gpu/drm/scheduler/sched_fence.o := MODULE_ENTRY = \
	-DLKPI_MODULE_INIT=lkpi_module_init_drm_sched \
	-DLKPI_MODULE_EXIT=lkpi_module_exit_drm_sched

$(OUT)/obj/illumos/%.o: $(UTSBASE)/intel/io/amdgpu/illumos/%.c $(CONFIG)
	@mkdir -p $(@D)
	@$(CHECK_FLAGS)
	$(COMPILE) -c $< -o $@

$(OUT)/obj/%.o: $(SRC)/contrib/linux/%.c $(CONFIG)
	@mkdir -p $(@D)
	@$(CHECK_FLAGS)
	$(COMPILE) $(LINUX_WFLAGS) $(MODULE_ENTRY) -c $< -o $@
