# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
all:
	@/bin/sh -n drmadm
include $(CODEMGR_WS)/usr/src/Makefile.master
include $(SRC)/Targetdirs

install: all directories
	cp -f drmadm $(ROOT)/usr/sbin/drmadm
	chmod 0555 $(ROOT)/usr/sbin/drmadm
	cp -f drmadm.8 $(ROOT)/usr/share/man/man8/drmadm.8
	cp -f $(SRC)/data/drm/drm-perms.xml $(ROOT)/lib/svc/manifest/system/drm-perms.xml
	cp -f $(SRC)/data/drm/20-modesetting.conf $(ROOT)/usr/share/drmadm/20-modesetting.conf
	cp -f $(UTSBASE)/common/io/virtio_gpu/virtio-gpu.aliases $(ROOT)/usr/share/drmadm/virtio-gpu.aliases
	cp -f $(UTSBASE)/intel/io/i915n/i915n.aliases $(ROOT)/usr/share/drmadm/i915n.aliases
	chmod 0444 $(ROOT)/usr/share/man/man8/drmadm.8 $(ROOT)/lib/svc/manifest/system/drm-perms.xml $(ROOT)/usr/share/drmadm/*
setup clean clobber:
