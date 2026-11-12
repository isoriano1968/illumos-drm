# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
all:
MODULE = cdrmnull
BUILD_MODULE = cdrmnull
FLAGS_DIR = $(UTSBASE)/common/io/cdrmnull
INSTALL_DIR = usr/kernel/drv/amd64
OUT = $(INTEL)/$(BUILD_MODULE)/obj64
include ../../Makefile.uts
OBJECTS = $(OUT)/cdrmnull.o
MAPFILES = $(UTSBASE)/common/io/cdrmnull/private.mapfile $(UTSBASE)/common/drm/drm_facade.mapfile
include ../../Makefile.targ

$(OUT)/cdrmnull.o: $(UTSBASE)/common/io/cdrmnull/cdrmnull.c $(CONFIG)
	@mkdir -p $(@D)
	@$(CHECK_FLAGS)
	$(COMPILE) -c $(UTSBASE)/common/io/cdrmnull/cdrmnull.c -o $@
