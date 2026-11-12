# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
all:
include $(CODEMGR_WS)/usr/src/Makefile.master
include $(SRC)/Targetdirs

GEN_LICENSE = $(UTSBASE)/intel/io/i915n/gen-license.sh
I915_INPUTS = $(INTEL)/i915n/display64/linux-inputs.txt
KMS_INPUTS = $(INTEL)/drm_kms/obj64/linux-inputs.txt

install: directories $(ROOT)/LICENSE.i915n $(ROOT)/LICENSE.drm_kms
	@sh $(BUILD_TOOLS)/stage.sh

$(ROOT)/LICENSE.i915n: $(I915_INPUTS) $(GEN_LICENSE)
	@mkdir -p $(ROOT)
	@if test -f $@; then chmod u+w $@; fi
	bash $(GEN_LICENSE) $(SRC)/contrib/linux $@ $(I915_INPUTS)
	chmod 0444 $@

$(ROOT)/LICENSE.drm_kms: $(KMS_INPUTS) $(GEN_LICENSE)
	@mkdir -p $(ROOT)
	@if test -f $@; then chmod u+w $@; fi
	MODULE=drm_kms bash $(GEN_LICENSE) $(SRC)/contrib/linux $@ $(KMS_INPUTS)
	chmod 0444 $@

package:
	@test -d $(ROOT) || { echo 'run dmake install first' >&2; exit 1; }
	@if test ! -f $(PKGREPO)/pkg5.repository; then pkgrepo create $(PKGREPO); pkgrepo -s $(PKGREPO) set publisher/prefix=illumos-drm; fi
	@set -e; for p in $(PACKAGES); do pkgsend publish -s $(PKGREPO) -d $(ROOT) $(SRC)/pkg/manifests/$$p.p5m; done
	@pkgrepo -s $(PKGREPO) list

setup clean:
clobber:
	$(RM) -r $(ROOT) $(PKGREPO) $(CARGO_TARGET_DIR)/x86_64-illumos-kmod
