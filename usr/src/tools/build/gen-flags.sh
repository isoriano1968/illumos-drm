#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
# Source flag definitions once and print make variable assignments.
set -eu
module=$1 OUT=$2
REPO=${CODEMGR_WS:?source myenv.sh first}
CC=${CC:-gcc} SYSINC=${SYSINC:-/usr/include} CROSS=0
LINUX=$REPO/usr/src/contrib/linux
LKPI=$REPO/usr/src/uts/common/io/linuxkpi
I915=$REPO/usr/src/uts/intel/io/i915n
FACADE_MAPFILE=$REPO/usr/src/uts/common/drm/drm_facade.mapfile
LKPI_MAPFILE=${LKPI_MAPFILE:-$REPO/usr/src/uts/intel/linuxkpi/obj64/linuxkpi.mapfile}
KMS_MAPFILE=$REPO/usr/src/uts/common/io/drm_kms/drm_kms.mapfile
case $module in
linuxkpi|linuxkpi_selftest) HERE=$LKPI ;;
drm_helpers|drm_helpers_test) HERE=$REPO/usr/src/uts/common/io/drm_helpers ;;
drm_kms) HERE=$REPO/usr/src/uts/common/io/drm_kms ;;
i915n) HERE=$I915; I915=$LINUX/drivers/gpu/drm/i915; I915_DISPLAY=${I915_DISPLAY:-1} ;;
cdrmnull) HERE=$REPO/usr/src/uts/common/io/cdrmnull; PRIVATE_MAPFILE=$HERE/private.mapfile ;;
drm_core) HERE=$REPO/usr/src/uts/common/io/drm/drm_core/ctf ;;
*) echo "unknown flag module: $module" >&2; exit 2 ;;
esac
mkdir -p "$OUT"
# The reference uses GNU ln semantics when refreshing directory symlinks.
ln() { /usr/gnu/bin/ln "$@"; }
die() { echo "flags: $*" >&2; exit 1; }
case $module in
cdrmnull|drm_core) ;;
*) . "$LKPI/kflags.sh" ;;
esac
. "$HERE/flags.sh"
case $module in
linuxkpi_selftest) CPPFLAGS=$STCPP; LDFLAGS=$STLDFLAGS ;;
drm_helpers_test) LDFLAGS=$TEST_LDFLAGS ;;
esac
flags_tmp=$(mktemp "$OUT/$module.flags.XXXXXX")
# Quotes must survive make's recipe expansion as literal compiler arguments.
# This escapes values, never selects or interprets shell program text.
value() { printf '%s' "$1" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/\$/$$/g' -e 's/#/\\#/g'; }
printvar() { printf '%s = ' "$1"; value "$2"; printf '\n'; }
{
    printvar FLAGS_TOKEN "$flags_tmp"
    printvar CFLAGS "${CFLAGS:-}"
    printvar CPPFLAGS "${CPPFLAGS:-}"
    printvar LDFLAGS "${LDFLAGS:-}"
    printvar BASENAME_CPPFLAG "${BASENAME_CPPFLAG:-}"
    printvar DEPFLAGS "${DEPFLAGS:-}"
} > "$flags_tmp"
mv "$flags_tmp" "$OUT/$module.flags.mk"
