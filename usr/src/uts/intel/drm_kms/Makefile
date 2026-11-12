# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
all:
MODULE = drm_kms
BUILD_MODULE = drm_kms
REF_SCRIPT = $(UTSBASE)/common/io/drm_kms/build.sh
INSTALL_DIR = kernel/misc/amd64
OUT = $(INTEL)/$(BUILD_MODULE)/obj64
include ../../Makefile.uts
include ../Makefile.files
OBJECTS = $(KMS_OBJECTS)
VPATH = $(KMS_DIRS)
MAPFILES = $(LKPI_MAPFILE) $(UTSBASE)/common/io/drm_kms/drm_kms.mapfile $(UTSBASE)/common/io/drm_kms/private.mapfile
SHARED_GATES = $(UTSBASE)/common/io/drm_kms/gates.sh
include ../../Makefile.targ
COMPILE = $(COMPILE.tu)

$(OUT)/obj/%.o: %.c $(CONFIG)
	@mkdir -p $(@D)
	$(COMPILE) -c $< -o $@
