# SPDX-License-Identifier: MIT
# Copyright 2026 The illumos-drm authors
# Sourced by the independent builder and the make flag generator.
CFLAGS=$LKPI_CFLAGS
CPPFLAGS="$LKPI_CPPFLAGS -DKBUILD_MODNAME=\"drm_kms\""
CPPFLAGS="$CPPFLAGS -nostdinc -I$REPO/usr/src/uts/common/io/linuxkpi/include -I$REPO/usr/src/uts/common"
CPPFLAGS="$CPPFLAGS -I$LINUX/include -I$LINUX/include/uapi"
CPPFLAGS="$CPPFLAGS -I$LINUX/drivers/gpu/drm -I$OUT/sysinc"
CPPFLAGS="$CPPFLAGS -include $I915/config.h -include $REPO/usr/src/uts/common/io/linuxkpi/include/linux/kconfig.h"
CPPFLAGS="$CPPFLAGS -include $REPO/usr/src/uts/common/io/linuxkpi/include/lkpi/prologue.h"
FACADE_MAPFILE=${FACADE_MAPFILE:-$REPO/usr/src/uts/common/drm/drm_facade.mapfile}
HELPERS_MAPFILE=${HELPERS_MAPFILE:-$REPO/usr/src/uts/common/io/drm_helpers/drm_helpers/drm_helpers.mapfile}
BASENAME_CPPFLAG='-DKBUILD_BASENAME='
DEPFLAGS='-MD -MF'
LDFLAGS="-ztype=kmod -N misc/drm_core -N misc/linuxkpi -N misc/drm_helpers -z defs -B reduce -M ${DDI_MAPFILE:-} -M $FACADE_MAPFILE -M ${LKPI_MAPFILE:-} -M $HELPERS_MAPFILE -M $HERE/private.mapfile -M $OUT/scope.mapfile"
