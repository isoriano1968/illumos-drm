# SPDX-License-Identifier: MIT
# Copyright 2026 The OpenIndiana SPARC desktop contributors
# Sourced by build.sh after linuxkpi's kflags.sh.
CFLAGS="$LKPI_CFLAGS -Wno-override-init"
CPPFLAGS="$LKPI_CPPFLAGS -DKBUILD_MODNAME=\"radeon\""
CPPFLAGS="$CPPFLAGS -nostdinc -I$REPO/usr/src/uts/common/io/linuxkpi/include -I$REPO/usr/src/uts/common"
CPPFLAGS="$CPPFLAGS -I$LINUX/include -I$LINUX/include/uapi"
CPPFLAGS="$CPPFLAGS -I$LINUX/drivers/gpu/drm/radeon -I$HERE/reg_safe"
CPPFLAGS="$CPPFLAGS -I$OUT/sysinc -I$HERE/illumos"
CPPFLAGS="$CPPFLAGS -include $HERE/config.h -include $REPO/usr/src/uts/common/io/linuxkpi/include/linux/kconfig.h"
CPPFLAGS="$CPPFLAGS -include $REPO/usr/src/uts/common/io/linuxkpi/include/lkpi/prologue.h"
# The byte order before any Linux header: radeon's __BIG_ENDIAN paths.
CPPFLAGS="$CPPFLAGS -include $REPO/usr/src/uts/common/io/linuxkpi/include/lkpi/byteorder.h"
FACADE_MAPFILE=${FACADE_MAPFILE:-$REPO/usr/src/uts/common/drm/drm_facade.mapfile}
HELPERS_MAPFILE=${HELPERS_MAPFILE:-$REPO/usr/src/uts/common/io/drm_helpers/drm_helpers/drm_helpers.mapfile}
KMS_MAPFILE=${KMS_MAPFILE:-$REPO/usr/src/uts/common/io/drm_kms/drm_kms.mapfile}
BASENAME_CPPFLAG='-DKBUILD_BASENAME='
DEPFLAGS='-MD -MF'
LDFLAGS="-ztype=kmod -N misc/drm_core -N misc/linuxkpi -N misc/drm_helpers -N misc/drm_kms -z defs -M ${DDI_MAPFILE:-} -M $FACADE_MAPFILE -M ${LKPI_MAPFILE:-} -M $HELPERS_MAPFILE -M $KMS_MAPFILE -M $HERE/private.mapfile"
