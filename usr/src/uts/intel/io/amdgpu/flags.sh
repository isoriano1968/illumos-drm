# SPDX-License-Identifier: MIT
# Copyright 2026 The illumos-drm authors
# Sourced by the make flag generator (usr/src/tools/build/gen-flags.sh).
AMDDIR=$LINUX/drivers/gpu/drm/amd
CFLAGS="$LKPI_CFLAGS -Wno-override-init"
CPPFLAGS="$LKPI_CPPFLAGS -DKBUILD_MODNAME=\"amdgpu\""
CPPFLAGS="$CPPFLAGS -nostdinc -I$REPO/usr/src/uts/common/io/linuxkpi/include -I$REPO/usr/src/uts/common"
CPPFLAGS="$CPPFLAGS -I$LINUX/include -I$LINUX/include/uapi"
# amdgpu's own include directories (its Makefiles' ccflags)
for d in include/asic_reg include amdgpu pm/inc pm/swsmu pm/swsmu/inc \
    pm/swsmu/inc/pmfw_if pm/swsmu/smu11 pm/swsmu/smu12 pm/swsmu/smu13 \
    pm/swsmu/smu14 pm/swsmu/smu15 pm/powerplay/inc pm/powerplay/smumgr \
    pm/powerplay/hwmgr pm/legacy-dpm acp/include display display/include \
    display/modules/inc display/dc display/amdgpu_dm amdkfd ras/ras_mgr \
    ras/rascore \
    display/dc/inc display/dc/inc/hw display/dc/clk_mgr display/dc/hwss \
    display/dc/resource display/dc/dsc display/dc/optc display/dc/dpp \
    display/dc/hubbub display/dc/dccg display/dc/hubp display/dc/dio \
    display/dc/dwb display/dc/hpo display/dc/mmhubbub display/dc/mpc \
    display/dc/opp display/dc/pg display/dc/soc_and_ip_translator \
    display/modules/freesync display/modules/color \
    display/modules/info_packet display/modules/power display/dmub/inc \
    display/modules/hdcp; do
	CPPFLAGS="$CPPFLAGS -I$AMDDIR/$d"
done
CPPFLAGS="$CPPFLAGS -I$OUT/sysinc -I$HERE/illumos"
CPPFLAGS="$CPPFLAGS -include $HERE/config.h -include $REPO/usr/src/uts/common/io/linuxkpi/include/linux/kconfig.h"
CPPFLAGS="$CPPFLAGS -include $REPO/usr/src/uts/common/io/linuxkpi/include/lkpi/prologue.h"
# __LITTLE_ENDIAN everywhere, as Linux's asm/byteorder.h gives it: the DC
# headers pick their bitfield layouts from it before any byte-order header.
CPPFLAGS="$CPPFLAGS -include $REPO/usr/src/uts/common/io/linuxkpi/include/lkpi/byteorder.h"
HELPERS_MAPFILE=${HELPERS_MAPFILE:-$REPO/usr/src/uts/common/io/drm_helpers/drm_helpers/drm_helpers.mapfile}
BASENAME_CPPFLAG='-DKBUILD_BASENAME='
DEPFLAGS='-MD -MF'
LDFLAGS="-ztype=kmod -N misc/drm_core -N misc/linuxkpi -N misc/drm_helpers -N misc/drm_kms -N misc/gfx_private -z defs -M ${DDI_MAPFILE:-} -M $FACADE_MAPFILE -M ${LKPI_MAPFILE:-} -M $HELPERS_MAPFILE -M $KMS_MAPFILE -M $HERE/private.mapfile"
