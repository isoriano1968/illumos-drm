# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors

CODEMGR_WS:sh= pwd
include usr/src/Makefile.master

all install check lint package setup: header
	@CODEMGR_WS=$(CODEMGR_WS) $(MAKE) -f usr/src/Makefile $@

clean clobber:
	@CODEMGR_WS=$(CODEMGR_WS) $(MAKE) -f usr/src/Makefile $@

header:
	@RUSTFLAGS='$(RUSTFLAGS)' sh $(BUILD_TOOLS)/environment.sh
