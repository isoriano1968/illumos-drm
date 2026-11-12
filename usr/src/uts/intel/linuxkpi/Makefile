# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
all:
MODULE = linuxkpi
BUILD_MODULE = linuxkpi
FLAGS_DIR = $(UTSBASE)/common/io/linuxkpi
SOURCE_DIR = $(UTSBASE)/common/io/linuxkpi/src
INSTALL_DIR = kernel/misc/amd64
OUT = $(INTEL)/$(BUILD_MODULE)/obj64
include ../../Makefile.uts
include ../Makefile.files
OBJECTS = $(LKPI_OBJECTS)
PRIVATE_COMMAND = ls $(UTSBASE)/common/io/linuxkpi/private.d/*.txt
MAPFILES = $(PRIVATE_COMMAND:sh)
include ../Makefile.rules
SHARED_GATES = $(UTSBASE)/common/io/linuxkpi/gates.sh
include ../../Makefile.targ

$(OUT)/linuxkpi.mapfile: $(OUT)/linuxkpi
	@sh $(BUILD_TOOLS)/gates.sh linuxkpi after $(OUT)
