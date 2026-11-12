#!/bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# Builds the cdrmnull kernel module on illumos: compiles cdrmnull.c with
# gcc and the illumos-gate's kernel flags (usr/src/Makefile.master,
# uts/Makefile.uts, uts/intel/Makefile.intel, as tools/cw translates them
# for gcc), then links it as a kernel module that depends on misc/drm_core and
# whose every undefined symbol is declared by a mapfile (-z defs).
#
#   usr/src/uts/common/io/cdrmnull/build.sh
#
# Environment (defaults in brackets):
#   CC           the compiler [gcc]
#   LD           the illumos link-editor [/usr/bin/ld]
#   REPO         the illumos-drm checkout [two directories above this script]
#   SYSINC       the system headers: sys/*.h, stdint.h and libdrm's
#                drm/drm.h (package developer/header-drm) [/usr/include]
#   DDI_MAPFILE  the gate's usr/src/uts/common/mapfiles/ddi.mapfile, which
#                an installed system does not have [./ddi.mapfile, else
#                ddi.mapfile next to this script]
#   OUT          where cdrmnull.o and the module cdrmnull go [.]
#
# The module is linked with
#   ld -ztype=kmod -N misc/drm_core -z defs -M ddi.mapfile \
#       -M usr/src/uts/common/drm/drm_facade.mapfile -M private.mapfile
# private.mapfile declares devmap_umem_remap, the one symbol outside the
# DDI, and the soft interrupt functions of the DDI that the gate's
# ddi.mapfile lacks.
#
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=${REPO:-$(cd "$HERE/../../../../../.." && pwd)}
CC=${CC:-gcc}
LD=${LD:-/usr/bin/ld}
SYSINC=${SYSINC:-/usr/include}
OUT=${OUT:-.}
FACADE_MAPFILE=$REPO/usr/src/uts/common/drm/drm_facade.mapfile
PRIVATE_MAPFILE=$HERE/private.mapfile
if [ -z "${DDI_MAPFILE:-}" ]; then
	if [ -f ./ddi.mapfile ]; then
		DDI_MAPFILE=./ddi.mapfile
	else
		DDI_MAPFILE=$HERE/ddi.mapfile
	fi
fi

die() {
	echo "build.sh: $*" >&2
	exit 1
}

for f in "$HERE/cdrmnull.c" "$HERE/cdrmnull_ioctl.h" \
    "$REPO/usr/src/uts/common/drm/drm_facade.h" "$FACADE_MAPFILE" "$PRIVATE_MAPFILE" \
    "$SYSINC/sys/sunddi.h" "$SYSINC/drm/drm.h"; do
	[ -f "$f" ] || die "missing $f"
done
[ -f "$DDI_MAPFILE" ] || die "missing $DDI_MAPFILE: copy the gate's" \
    "usr/src/uts/common/mapfiles/ddi.mapfile there or set DDI_MAPFILE"
mkdir -p "$OUT"

. "$HERE/flags.sh"

echo "\$ $CC --version"
"$CC" --version | sed -n 1p
echo "\$ $CC $CFLAGS $CPPFLAGS -c $HERE/cdrmnull.c -o $OUT/cdrmnull.o"
# shellcheck disable=SC2086 # word splitting of the flag lists
"$CC" $CFLAGS $CPPFLAGS -c "$HERE/cdrmnull.c" -o "$OUT/cdrmnull.o"

echo "\$ $LD $LDFLAGS -o $OUT/cdrmnull $OUT/cdrmnull.o"
"$LD" $LDFLAGS -o "$OUT/cdrmnull" "$OUT/cdrmnull.o"

if command -v elfdump >/dev/null 2>&1; then
	echo "\$ elfdump -d $OUT/cdrmnull | grep NEEDED"
	elfdump -d "$OUT/cdrmnull" | grep NEEDED || true
fi
if command -v digest >/dev/null 2>&1; then
	echo "\$ digest -a sha256 $OUT/cdrmnull"
	digest -a sha256 "$OUT/cdrmnull" || true
fi
echo "build.sh: built $OUT/cdrmnull (object $OUT/cdrmnull.o)"
