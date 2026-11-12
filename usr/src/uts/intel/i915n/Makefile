# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
all:
MODULE = i915n
BUILD_MODULE = i915n
VARIANT = display
DISPLAY = 1
REF_SCRIPT = $(UTSBASE)/intel/io/i915n/build.sh
INSTALL_DIR = kernel/drv/amd64
OUT = $(INTEL)/i915n/$(VARIANT)64
include ../../Makefile.uts
include ../Makefile.files
OBJECTS = $(I915_OBJECTS)
SOURCE_DIR = $(SRC)/contrib/linux/drivers/gpu/drm/i915
MAPFILES = $(LKPI_MAPFILE) $(UTSBASE)/intel/io/i915n/private.mapfile $(UTSBASE)/common/io/drm_kms/drm_kms.mapfile
include ../Makefile.rules
include ../../Makefile.targ
COMPILE = $(COMPILE.tu)

$(OUT)/obj/illumos/%.o: $(UTSBASE)/intel/io/i915n/illumos/%.c $(CONFIG)
	@mkdir -p $(@D)
	$(COMPILE) -c $< -o $@
