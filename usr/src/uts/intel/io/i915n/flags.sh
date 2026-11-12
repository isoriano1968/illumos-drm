# SPDX-License-Identifier: MIT
# Copyright 2026 The illumos-drm authors
# Sourced by the independent builder and the make flag generator.
CFLAGS=$LKPI_CFLAGS
CPPFLAGS="$LKPI_CPPFLAGS -DI915 -DKBUILD_MODNAME=\"i915\""
CPPFLAGS="$CPPFLAGS -nostdinc -I$REPO/usr/src/uts/common/io/linuxkpi/include -I$REPO/usr/src/uts/common"
CPPFLAGS="$CPPFLAGS -I$LINUX/include -I$LINUX/include/uapi"
CPPFLAGS="$CPPFLAGS -I$I915 -I$OUT/sysinc"
# Last: the illumos glue's headers the patches include (i915_illumos_vbt.h).
CPPFLAGS="$CPPFLAGS -I$HERE/illumos"
CPPFLAGS="$CPPFLAGS -include $HERE/config.h -include $REPO/usr/src/uts/common/io/linuxkpi/include/linux/kconfig.h"
CPPFLAGS="$CPPFLAGS -include $REPO/usr/src/uts/common/io/linuxkpi/include/lkpi/prologue.h"
case "$I915_DISPLAY" in
1) CPPFLAGS="$CPPFLAGS -DI915N_DISPLAY" ;;
0) ;;
*) echo "build.sh: I915_DISPLAY must be 0 or 1" >&2; exit 1 ;;
esac
HELPERS_MAPFILE=${HELPERS_MAPFILE:-$REPO/usr/src/uts/common/io/drm_helpers/drm_helpers/drm_helpers.mapfile}
BASENAME_CPPFLAG='-DKBUILD_BASENAME='
DEPFLAGS='-MD -MF'
kms=""
if [ "$I915_DISPLAY" = 1 ]; then
	kms="-N misc/drm_kms -N misc/gfx_private -M $KMS_MAPFILE"
fi
LDFLAGS="-ztype=kmod -N misc/drm_core -N misc/linuxkpi -N misc/drm_helpers $kms -z defs -M ${DDI_MAPFILE:-} -M $REPO/usr/src/uts/common/drm/drm_facade.mapfile -M ${LKPI_MAPFILE:-} -M $HELPERS_MAPFILE -M $HERE/private.mapfile"
