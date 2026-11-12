# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
all:
MODULE = drm_core
CRATE = drm-module
BUILD_MODULE = drm_core
REF_SCRIPT = $(UTSBASE)/common/io/drm/drm_core/ctf/build-ctf.sh
INSTALL_DIR = kernel/misc/amd64
OUT = $(INTEL)/$(BUILD_MODULE)/obj64
include ../../Makefile.uts
ARCHIVE = $(CARGO_TARGET_DIR)/x86_64-illumos-kmod/release/drm_module.a
CTF = $(OUT)/drm_facade_ctf.o
SCOPE = $(UTSBASE)/common/io/drm/drm_core/drm_core.mapfile

all: cargo .WAIT $(OUT)/drm_core
cargo:
	@cd $(CODEMGR_WS) && RUSTFLAGS='$(RUSTFLAGS)' $(CARGO_KMOD)

$(ARCHIVE):
	@test -f $@ || { echo 'run dmake all to build the Rust archive' >&2; exit 1; }
$(CTF): $(UTSBASE)/common/io/drm/drm_core/ctf/drm_facade_ctf.c $(CONFIG)
	@mkdir -p $(@D)
	$(COMPILE) -c $(UTSBASE)/common/io/drm/drm_core/ctf/drm_facade_ctf.c -o $@
	$(CTFCONVERT) -l drm $@
	@set -e; for s in `elfdump -c $@ | awk '/sh_name: \.debug/ {print $$4}'`; do mcs -d -n $$s $@; done

$(OUT)/drm_core: $(ARCHIVE) $(CTF) $(SCOPE) $(BUILD_TOOLS)/rust-link.sh
	@LD=$(LD) sh $(BUILD_TOOLS)/rust-link.sh drm_core $(ARCHIVE) $@ $(CTF)
	@sh $(BUILD_TOOLS)/gates.sh drm_core after $(OUT)

install: all
	@mkdir -p $(ROOT)/$(INSTALL_DIR)
	@cp $(OUT)/drm_core $(ROOT)/$(INSTALL_DIR)/drm_core
	@chmod 0755 $(ROOT)/$(INSTALL_DIR)/drm_core
clean:
	$(RM) $(CTF) $(CTF).argv $(CTF).command $(OUT)/drm_core $(OUT)/.make.state
clobber: clean
	$(RM) -r $(OUT)
