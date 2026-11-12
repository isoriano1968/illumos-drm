# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
all:
MODULE = drm_helpers_test
BUILD_MODULE = drm_helpers
REF_SCRIPT = $(UTSBASE)/common/io/drm_helpers/build.sh
INSTALL_DIR = usr/kernel/drv/amd64
OUT = $(INTEL)/$(BUILD_MODULE)/obj64
include ../../Makefile.uts
HELPERS = $(UTSBASE)/common/io/drm_helpers
OBJECTS = $(OUT)/obj/drm_helpers_test.o $(OUT)/obj/selftest_kernel.o $(OUT)/obj/drm_mm_selftest.o $(OUT)/obj/drm_cache_selftest.o
MAPFILES = $(LKPI_MAPFILE) $(HELPERS)/drm_helpers/drm_helpers.mapfile
include ../../Makefile.targ

$(OUT)/obj/drm_helpers_test.o: $(HELPERS)/test/drm_helpers_test.c $(CONFIG)
	@mkdir -p $(@D)
	$(COMPILE) -c $(HELPERS)/test/drm_helpers_test.c -o $@
$(OUT)/obj/selftest_kernel.o: $(HELPERS)/test/selftest_kernel.c $(CONFIG)
	@mkdir -p $(@D)
	$(COMPILE) -c $(HELPERS)/test/selftest_kernel.c -o $@
$(OUT)/obj/drm_mm_selftest.o: $(HELPERS)/drm_mm/tests/drm_mm_selftest.c $(CONFIG)
	@mkdir -p $(@D)
	$(COMPILE) -c $(HELPERS)/drm_mm/tests/drm_mm_selftest.c -o $@
$(OUT)/obj/drm_cache_selftest.o: $(HELPERS)/drm_cache/tests/drm_cache_selftest.c $(CONFIG)
	@mkdir -p $(@D)
	$(COMPILE) -c $(HELPERS)/drm_cache/tests/drm_cache_selftest.c -o $@
