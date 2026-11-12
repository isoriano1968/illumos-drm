# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
all:
MODULE = drm_helpers
BUILD_MODULE = drm_helpers
FLAGS_DIR = $(UTSBASE)/common/io/drm_helpers
INSTALL_DIR = kernel/misc/amd64
OUT = $(INTEL)/$(BUILD_MODULE)/obj64
include ../../Makefile.uts
OBJECTS = $(OUT)/obj/drm_mm.o $(OUT)/obj/drm_cache.o $(OUT)/obj/drm_helpers_mod.o
MAPFILES = $(LKPI_MAPFILE) $(UTSBASE)/common/io/drm_helpers/drm_helpers/private.mapfile $(UTSBASE)/common/io/drm_helpers/drm_helpers/drm_helpers_scope.mapfile
SOURCE_DIR = $(OUT)/src/linux/drivers/gpu/drm
include ../Makefile.rules
SHARED_GATES = $(UTSBASE)/common/io/drm_helpers/gates.sh
include ../../Makefile.targ

$(OUT)/src/linux/drivers/gpu/drm/drm_mm.c $(OUT)/src/linux/drivers/gpu/drm/drm_cache.c: $(OUT)/staged
$(OUT)/staged: $(SRC)/contrib/linux/drivers/gpu/drm/drm_mm.c $(SRC)/contrib/linux/drivers/gpu/drm/drm_cache.c $(SRC)/contrib/linux/include/drm/drm_mm.h $(SRC)/contrib/linux/include/drm/drm_cache.h
	@sh $(UTSBASE)/common/io/drm_helpers/prepare-sources.sh $(OUT)/src
	@touch $@

$(OUT)/obj/drm_helpers_mod.o: $(UTSBASE)/common/io/drm_helpers/drm_helpers/drm_helpers_mod.c $(CONFIG)
	@mkdir -p $(@D)
	@$(CHECK_FLAGS)
	$(COMPILE) -c $(UTSBASE)/common/io/drm_helpers/drm_helpers/drm_helpers_mod.c -o $@
