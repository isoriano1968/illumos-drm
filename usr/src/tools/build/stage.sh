#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
# Preserve source operands of the unchanged manifests inside the proto area too.
set -eu
repo=${CODEMGR_WS:?} root=${ROOT:?}
copy() {
    mkdir -p "$root/$(dirname "$2")"
    cp -f "$repo/$1" "$root/$2"
    chmod "$3" "$root/$2"
}
copy LICENSE LICENSE 0444
for f in usr/src/uts/common/drm/LICENSE \
    usr/src/uts/common/io/drm_helpers/drm_mm/LICENSE \
    usr/src/uts/common/io/drm_helpers/drm_cache/LICENSE \
    usr/src/data/firmware/LICENSE.i915 \
    usr/src/data/firmware/LICENSE.amdgpu; do
    copy "$f" "$f" 0444
done
for pair in \
    usr/src/uts/intel/io/i915n/i915n.conf:kernel/drv/i915n.conf \
    usr/src/uts/intel/io/amdgpu/amdgpu.conf:kernel/drv/amdgpu.conf \
    usr/src/uts/common/io/cdrmnull/cdrmnull.conf:usr/kernel/drv/cdrmnull.conf; do
    copy "${pair%:*}" "${pair#*:}" 0644
done
# Explicit operands and installed paths are both populated.
for f in usr/src/data/drm/20-modesetting.conf \
    usr/src/uts/common/io/virtio_gpu/virtio-gpu.aliases \
    usr/src/uts/intel/io/i915n/i915n.aliases \
    usr/src/uts/intel/io/i915n/i915n.conf \
    usr/src/uts/intel/io/amdgpu/amdgpu.conf \
    usr/src/uts/intel/io/amdgpu/amdgpu.system \
    usr/src/data/firmware/i915/skl_dmc_ver1_27.bin \
    usr/src/data/firmware/i915/kbl_dmc_ver1_04.bin; do
    copy "$f" "$f" 0444
done
for f in skl_dmc_ver1_27.bin kbl_dmc_ver1_04.bin; do
    copy "usr/src/data/firmware/i915/$f" "kernel/firmware/i915/$f" 0444
done
# amdgpu's firmware is not in Git (usr/src/data/firmware/README.md): every
# blob the manifest names must be present.
grep '^file usr/src/data/firmware/amdgpu/' "$repo/usr/src/pkg/manifests/amdgpu.p5m" |
    awk '{print $2}' | while read -r f; do
    [ -f "$repo/$f" ] || { echo "stage: missing amdgpu firmware $f" >&2; exit 1; }
    copy "$f" "$f" 0444
    copy "$f" "kernel/firmware/amdgpu/${f##*/}" 0444
done
