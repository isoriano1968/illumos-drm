# SPDX-License-Identifier: MIT
# Copyright 2026 The illumos-drm authors
# Sourced by the independent builder and the make flag generator.
CFLAGS=$LKPI_CFLAGS
CPPFLAGS="$LKPI_CPPFLAGS -DKBUILD_MODNAME=\"linuxkpi\""
CPPFLAGS="$CPPFLAGS -nostdinc -I$HERE/include -I$REPO/usr/src/uts/common -I$HERE/src -I$OUT/sysinc"
CPPFLAGS="$CPPFLAGS -include $HERE/include/linux/kconfig.h -include $HERE/include/lkpi/prologue.h"
STCPP="$CPPFLAGS -UKBUILD_MODNAME -DKBUILD_MODNAME=\"linuxkpi_selftest\""
LDFLAGS="-ztype=kmod -N misc/drm_core -z defs -M ${DDI_MAPFILE:-} -M $FACADE_MAPFILE -M $OUT/private.mapfile"
STLDFLAGS="-ztype=kmod -N misc/linuxkpi -N misc/drm_core -z defs -M ${DDI_MAPFILE:-} -M $FACADE_MAPFILE -M $OUT/linuxkpi.mapfile"
