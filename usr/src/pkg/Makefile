# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
all:
include $(CODEMGR_WS)/usr/src/Makefile.master
include $(SRC)/Targetdirs

GEN_LICENSE = $(UTSBASE)/intel/io/i915n/gen-license.sh
I915_INPUTS = $(INTEL)/i915n/display64/linux-inputs.txt
KMS_INPUTS = $(INTEL)/drm_kms/obj64/linux-inputs.txt
AMDGPU_INPUTS = $(INTEL)/amdgpu/obj64/linux-inputs.txt
PKG_SERIAL:sh= printf '%s' "${PKG_SERIAL-}"
PKG_PUBLISHER:sh= printf '%s' "${PKG_PUBLISHER:-illumos-drm}"
PKG_MANIFESTS = $(PKGREPO).manifests

install: directories $(ROOT)/LICENSE.i915n $(ROOT)/LICENSE.drm_kms $(ROOT)/LICENSE.amdgpu
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

$(ROOT)/LICENSE.amdgpu: $(AMDGPU_INPUTS) $(GEN_LICENSE)
	@mkdir -p $(ROOT)
	@if test -f $@; then chmod u+w $@; fi
	MODULE=amdgpu bash $(GEN_LICENSE) $(SRC)/contrib/linux $@ $(AMDGPU_INPUTS)
	chmod 0444 $@

package:
	@test -d $(ROOT) || { echo 'run dmake install first' >&2; exit 1; }
	@case '$(PKG_SERIAL)' in *[!0-9]*) echo 'PKG_SERIAL must be decimal digits or empty' >&2; exit 1 ;; esac
	@if test ! -f $(PKGREPO)/pkg5.repository; then pkgrepo create $(PKGREPO); fi
	@pkgrepo -s $(PKGREPO) set publisher/prefix=$(PKG_PUBLISHER)
	@mkdir -p $(PKG_MANIFESTS)
	@set -e; for p in $(PACKAGES); do \
	    transforms='$(SRC)/pkg/publisher.mog'; \
	    if test -n '$(PKG_SERIAL)'; then transforms="$$transforms $(SRC)/pkg/serial.mog"; fi; \
	    pkgmogrify -D PKG_SERIAL='$(PKG_SERIAL)' -D PKG_PUBLISHER='$(PKG_PUBLISHER)' \
	        $(SRC)/pkg/manifests/$$p.p5m $$transforms > $(PKG_MANIFESTS)/$$p.p5m; \
	    pkgsend publish -s $(PKGREPO) -d $(ROOT) $(PKG_MANIFESTS)/$$p.p5m; \
	done
	@pkgrepo -s $(PKGREPO) list

setup clean:
clobber:
	$(RM) -r $(ROOT) $(PKGREPO) $(PKG_MANIFESTS) $(CARGO_TARGET_DIR)/x86_64-illumos-kmod
