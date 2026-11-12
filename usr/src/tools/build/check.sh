#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
set -eu
repo=${CODEMGR_WS:?}
cd "$repo"
b=$repo/usr/src/uts/intel
out=$repo/target/check
mkdir -p "$out"
sh usr/src/contrib/linux/verify-vendored.sh -n
for pair in drm_kms:obj64 i915n:display64 i915n:headless64; do
    m=${pair%:*} d=$b/${pair%:*}/${pair#*:}
    python3 usr/src/uts/intel/io/i915n/check-licences.py usr/src/contrib/linux "$d/linux-inputs.txt"
    if [ "$m" = drm_kms ]; then
        (cd "$b/$m" && /usr/bin/dmake check-gates)
    else
        variant=${pair#*:}; variant=${variant%64}
        display=1; [ "$variant" != headless ] || display=0
        (cd "$b/$m" && /usr/bin/dmake VARIANT="$variant" DISPLAY="$display" check-gates)
    fi
done
for m in drm_core virtio_gpu linuxkpi drm_helpers drm_kms; do
    sh usr/src/tools/build/gates.sh "$m" after "$b/$m/obj64"
done
needed() {
    file=$1; shift
    elfdump -d "$file" | awk '$2 == "NEEDED" {print $4}' | sort > "$out/actual"
    printf '%s\n' "$@" | sort > "$out/expected"
    diff "$out/expected" "$out/actual"
    elfdump -d "$file" | grep SUNW_KMOD
}
needed "$b/drm_core/obj64/drm_core" fs/specfs
needed "$b/virtio_gpu/obj64/virtio_gpu" misc/virtio fs/specfs misc/drm_core
needed "$b/virtio_gpu/obj64/rdrmnull" fs/specfs misc/drm_core
needed "$b/linuxkpi/obj64/linuxkpi" misc/drm_core
needed "$b/linuxkpi/obj64/linuxkpi_selftest" misc/linuxkpi misc/drm_core
needed "$b/drm_helpers/obj64/drm_helpers" misc/linuxkpi
needed "$b/drm_helpers/obj64/drm_helpers_test" misc/drm_helpers misc/linuxkpi
needed "$b/drm_kms/obj64/drm_kms" misc/drm_core misc/linuxkpi misc/drm_helpers
needed "$b/i915n/display64/i915n" misc/drm_core misc/linuxkpi misc/drm_helpers misc/drm_kms misc/gfx_private
needed "$b/i915n/headless64/i915n" misc/drm_core misc/linuxkpi misc/drm_helpers
needed "$b/cdrmnull/obj64/cdrmnull" misc/drm_core
sh usr/src/uts/intel/io/i915n/gen-aliases.sh usr/src/contrib/linux > "$out/i915n.aliases"
cmp "$out/i915n.aliases" usr/src/uts/intel/io/i915n/i915n.aliases
CC=${CC:-gcc} OUT=$out/linuxkpi sh usr/src/uts/common/io/linuxkpi/tests/host/host-test.sh
CC=${CC:-gcc} OUT=$out/helpers sh usr/src/uts/common/io/drm_helpers/host-test.sh
sh usr/src/test/drm-tests/test-drmadm-aliases.sh
CC=${CC:-gcc} sh usr/src/test/drm-tests/test-hotplug-deadline.sh
/usr/sbin/svccfg validate usr/src/data/drm/drm-perms.xml
# Retain the native layout comparison previously run by the illumos CI job.
REPO=$repo
. "$repo/usr/src/uts/common/io/drm/drm_core/ctf/flags.sh"
"${CC:-gcc}" $FACADE_CHECK_FLAGS -S -o "$out/facade.s" usr/src/tools/facade/facade_layout.c
sed -n 's/^.*@@ //p' "$out/facade.s" | tr -d '"' > "$out/facade-gcc.out"
if [ "${RUST_TOOLCHAIN:-nightly-2026-04-16}" = system ]; then
    "${CARGO:-cargo}" run ${CARGO_FLAGS:---locked} -j "${CARGO_BUILD_JOBS:-2}" -p xtask -- facade-check "$out/facade-gcc.out"
else
    "${CARGO:-cargo}" +"${RUST_TOOLCHAIN:-nightly-2026-04-16}" run ${CARGO_FLAGS:---locked} -j "${CARGO_BUILD_JOBS:-2}" -p xtask -- facade-check "$out/facade-gcc.out"
fi
