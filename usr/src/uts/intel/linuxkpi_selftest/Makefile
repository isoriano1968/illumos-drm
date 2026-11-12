# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
all:
MODULE = linuxkpi_selftest
BUILD_MODULE = linuxkpi
FLAGS_DIR = $(UTSBASE)/common/io/linuxkpi
INSTALL_DIR = usr/kernel/drv/amd64
OUT = $(INTEL)/$(BUILD_MODULE)/obj64
include ../../Makefile.uts
include ../Makefile.files
OBJECTS = $(SELFTEST_OBJECTS)
MAPFILES = $(LKPI_MAPFILE)
include ../Makefile.rules
include ../../Makefile.targ
