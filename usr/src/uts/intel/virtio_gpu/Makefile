# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
all:
MODULE = virtio_gpu
CRATE = virtio_gpu
BUILD_MODULE = virtio_gpu
include $(CODEMGR_WS)/usr/src/Makefile.master
OUT = $(INTEL)/virtio_gpu/obj64
ARCHIVE = $(CARGO_TARGET_DIR)/x86_64-illumos-kmod/release/virtio_gpu.a
.KEEP_STATE:
.KEEP_STATE_FILE: .make.state.$(MODULE).$(VARIANT)

all: cargo .WAIT $(OUT)/virtio_gpu .WAIT rdrmnull
cargo:
	@cd $(CODEMGR_WS) && RUSTFLAGS='$(RUSTFLAGS)' $(CARGO_KMOD)
$(ARCHIVE):
	@test -f $@ || { echo 'run dmake all to build the Rust archive' >&2; exit 1; }
$(OUT)/virtio_gpu: $(ARCHIVE) $(BUILD_TOOLS)/rust-link.sh
	@mkdir -p $(OUT)
	@LD=$(LD) sh $(BUILD_TOOLS)/rust-link.sh virtio_gpu $(ARCHIVE) $@
	@sh $(BUILD_TOOLS)/gates.sh virtio_gpu after $(OUT)
install: all
	@mkdir -p $(ROOT)/kernel/drv/amd64
	@cp $(OUT)/virtio_gpu $(ROOT)/kernel/drv/amd64/virtio_gpu
	@chmod 0755 $(ROOT)/kernel/drv/amd64/virtio_gpu
setup:
	@sh $(BUILD_TOOLS)/setup.sh
	@mkdir -p $(OUT)
clean:
	$(RM) $(OUT)/virtio_gpu $(OUT)/rdrmnull $(OUT)/virtio_gpu.core $(OUT)/rdrmnull.core
clobber: clean
	$(RM) -r $(OUT)

# Preserve the additional Rust test-driver build and core-symbol gate in CI.
rdrmnull: cargo-null .WAIT $(OUT)/rdrmnull
cargo-null:
	@cd $(CODEMGR_WS) && RUSTFLAGS='$(RUSTFLAGS)' $(CARGO_BUILD) -p rdrmnull $(CARGO_TARGET)
$(CARGO_TARGET_DIR)/x86_64-illumos-kmod/release/rdrmnull.a:
	@test -f $@ || { echo 'run dmake all to build the Rust archive' >&2; exit 1; }
$(OUT)/rdrmnull: $(CARGO_TARGET_DIR)/x86_64-illumos-kmod/release/rdrmnull.a $(BUILD_TOOLS)/rust-link.sh
	@mkdir -p $(OUT)
	@LD=$(LD) sh $(BUILD_TOOLS)/rust-link.sh rdrmnull $(CARGO_TARGET_DIR)/x86_64-illumos-kmod/release/rdrmnull.a $@
	@sh $(BUILD_TOOLS)/gates.sh rdrmnull after $(OUT)

flags:
