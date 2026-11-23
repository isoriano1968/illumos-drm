# SPDX-License-Identifier: MIT
# Copyright 2026 The illumos-drm authors
# Sourced by the independent builder and the make flag generator.
CFLAGS=$LKPI_CFLAGS
CPPFLAGS="$LKPI_CPPFLAGS -DKBUILD_MODNAME=\"linuxkpi\""
CPPFLAGS="$CPPFLAGS -nostdinc -I$HERE/include -I$REPO/usr/src/uts/common -I$HERE/src -I$OUT/sysinc"
CPPFLAGS="$CPPFLAGS -include $HERE/include/linux/kconfig.h -include $HERE/include/lkpi/prologue.h"
STCPP="$CPPFLAGS -UKBUILD_MODNAME -DKBUILD_MODNAME=\"linuxkpi_selftest\""
# misc/acpica and misc/gfx_private are x86 modules; aarch64 has no ACPICA,
# sparcv9 neither (and carries its own two gfx_private calls, lkpi_page.c).
case ${LKPI_ARCH:-x86_64} in
aarch64) LKPI_XDEPS="-N misc/gfx_private " ;;
sparcv9) LKPI_XDEPS= ;;
*) LKPI_XDEPS="-N misc/acpica -N misc/gfx_private " ;;
esac
LDFLAGS="-ztype=kmod -N misc/drm_core ${LKPI_XDEPS}-z defs -M ${DDI_MAPFILE:-} -M $FACADE_MAPFILE -M $OUT/private.mapfile"
STLDFLAGS="-ztype=kmod -N misc/linuxkpi -N misc/drm_core -z defs -M ${DDI_MAPFILE:-} -M $FACADE_MAPFILE -M $OUT/linuxkpi.mapfile"
