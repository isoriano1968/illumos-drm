# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
include $(CODEMGR_WS)/usr/src/Makefile.master
include $(SRC)/Targetdirs

all install clean clobber setup:
	@set -e; for m in $(MODULES); do \
	    (cd $(INTEL)/$$m && if test $@ = all -o $@ = install; then $(MAKE) flags; fi && $(MAKE) -m parallel -j $(DMAKE_MAX_JOBS) $@); \
	    if test $$m = i915n; then t=$@; if test $$t = install; then t=all; fi; (cd $(INTEL)/i915n && if test $$t = all; then $(MAKE) VARIANT=headless DISPLAY=0 flags; fi && $(MAKE) -m parallel -j $(DMAKE_MAX_JOBS) VARIANT=headless DISPLAY=0 $$t); fi; \
	done
	@cd $(SRC)/cmd/drmadm && $(MAKE) $@
	@cd $(SRC)/pkg && $(MAKE) $@

check:
	@sh $(BUILD_TOOLS)/check.sh

lint:
	@set -e; for d in . usr/src usr/src/cmd/drmadm usr/src/pkg; do \
	    (cd $(CODEMGR_WS)/$$d && $(MAKE) -n all); \
	done; for m in $(MODULES); do \
	    (cd $(INTEL)/$$m && $(MAKE) -n all); \
	done
	@cd $(INTEL)/i915n && $(MAKE) -n VARIANT=headless DISPLAY=0 all
	@pkglint $(SRC)/pkg/manifests/drm-core.p5m $(SRC)/pkg/manifests/drm-helpers.p5m $(SRC)/pkg/manifests/drm-kms.p5m $(SRC)/pkg/manifests/linuxkpi.p5m $(SRC)/pkg/manifests/i915n.p5m $(SRC)/pkg/manifests/virtio-gpu.p5m $(SRC)/pkg/manifests/drmadm.p5m

package:
	@cd $(SRC)/pkg && $(MAKE) package
