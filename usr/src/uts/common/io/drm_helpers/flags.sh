# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
# Sourced by the independent builder and the make flag generator.
CFLAGS=$LKPI_CFLAGS
# The Linux world: the carried headers, then linuxkpi's, then the
# facade's drm/*.h, then the kernel's system headers, with the layer's
# prologue. drm_cache.c builds unmodified (the
# layer's page, scatterlist and iosys-map models exist since R5c).
CPPFLAGS="$LKPI_CPPFLAGS -DKBUILD_MODNAME=\"drm_helpers\""
CPPFLAGS="$CPPFLAGS -DCONFIG_X86=1 -DCONFIG_X86_64=1"
CPPFLAGS="$CPPFLAGS -nostdinc -I$OUT/src/linux/include -I$REPO/usr/src/uts/common/io/linuxkpi/include"
CPPFLAGS="$CPPFLAGS -I$REPO/usr/src/uts/common -I$HERE/test -I$OUT/sysinc"
CPPFLAGS="$CPPFLAGS -include $REPO/usr/src/uts/common/io/linuxkpi/include/linux/kconfig.h"
CPPFLAGS="$CPPFLAGS -include $REPO/usr/src/uts/common/io/linuxkpi/include/lkpi/prologue.h"
SCOPE=$HERE/drm_helpers/drm_helpers_scope.mapfile
LDFLAGS="-ztype=kmod -N misc/linuxkpi -z defs -B reduce -M $DDI_MAPFILE -M ${LKPI_MAPFILE:-} -M $HERE/drm_helpers/private.mapfile -M $SCOPE"
TEST_LDFLAGS="-ztype=kmod -N misc/drm_helpers -N misc/linuxkpi -z defs -M $DDI_MAPFILE -M ${LKPI_MAPFILE:-} -M $HERE/drm_helpers/drm_helpers.mapfile"
