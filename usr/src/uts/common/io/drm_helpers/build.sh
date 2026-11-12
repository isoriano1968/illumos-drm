#!/bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# Builds misc/drm_helpers (the carried Linux helpers) and the test driver
# drm_helpers_test on illumos, with gcc and the kernel flags of the
# LinuxKPI layer (usr/src/uts/common/io/linuxkpi/kflags.sh, the illumos-gate's 64-bit kernel
# flags plus the Linux dialect):
#
#   usr/src/uts/common/io/drm_helpers/build.sh
#
# 1. usr/src/uts/common/io/drm_helpers/prepare-sources.sh stages the four vendored helper files into
#    $OUT/src/linux, preserving the helpers' isolated include directory.
# 2. drm_mm.c and drm_cache.c (Linux) and the module linkage are compiled
#    against the carried headers, usr/src/uts/common/io/linuxkpi/include (with the layer's
#    prologue), the facade (include/) and the system headers, and linked
#      ld -ztype=kmod -N misc/linuxkpi -z defs -B reduce -M ddi.mapfile \
#          -M linuxkpi.mapfile -M drm_helpers/private.mapfile \
#          -M drm_helpers/drm_helpers_scope.mapfile
#    The module's global symbols must be exactly the scope mapfile's (else
#    the build fails), so i915 can bind to drm_mm and nothing else. The
#    red-black trees, processor facts, pages and scatterlists it uses are
#    misc/linuxkpi's (R5c); before R5c they were compiled into the module.
# 3. The test driver is linked
#      ld -ztype=kmod -N misc/drm_helpers -N misc/linuxkpi -z defs \
#          -M ddi.mapfile -M linuxkpi.mapfile -M drm_helpers/drm_helpers.mapfile
#    i.e. against the exported functions only, as a C driver would be.
#
# Environment (defaults in brackets):
#   CC           the compiler [gcc]
#   LD           the illumos link-editor [/usr/bin/ld]
#   SYSINC       the system headers, with libdrm's drm/drm.h [/usr/include]
#   DDI_MAPFILE  the gate's usr/src/uts/common/mapfiles/ddi.mapfile
#                [./ddi.mapfile, else usr/src/uts/common/io/cdrmnull/ddi.mapfile]
#   LKPI_MAPFILE linuxkpi.mapfile from usr/src/uts/common/io/linuxkpi/build.sh [required to link]
#   OUT          where the objects and the modules go [./target/carried]
#   CROSS        1 to compile only, with a Linux-hosted cross gcc
#                (development; see usr/src/uts/common/io/linuxkpi/kflags.sh)
#
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../../../../../.." && pwd)
CC=${CC:-gcc}
LD=${LD:-/usr/bin/ld}
SYSINC=${SYSINC:-/usr/include}
OUT=${OUT:-./target/carried}
if [ -z "${DDI_MAPFILE:-}" ]; then
	if [ -f ./ddi.mapfile ]; then
		DDI_MAPFILE=./ddi.mapfile
	else
		DDI_MAPFILE=$REPO/usr/src/uts/common/io/cdrmnull/ddi.mapfile
	fi
fi

die() {
	echo "build.sh: $*" >&2
	exit 1
}

[ -f "$DDI_MAPFILE" ] || die "missing $DDI_MAPFILE: copy the gate's" \
    "usr/src/uts/common/mapfiles/ddi.mapfile there or set DDI_MAPFILE"
for f in "$SYSINC/sys/sunddi.h" "$SYSINC/drm/drm.h" \
    "$REPO/usr/src/uts/common/drm/drm_facade.h"; do
	[ -f "$f" ] || die "missing $f"
done
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)

sh "$HERE/prepare-sources.sh" "$OUT/src"

REPO_SAVED=$REPO
# shellcheck disable=SC1091
. "$REPO/usr/src/uts/common/io/linuxkpi/kflags.sh"
REPO=$REPO_SAVED
. "$HERE/flags.sh"

cc1() {
	echo "\$ $CC ... -c $1 -o $2"
	# shellcheck disable=SC2086 # word splitting of the flag lists
	"$CC" $CFLAGS $CPPFLAGS -c "$1" -o "$2"
}

echo "\$ $CC --version"
"$CC" --version | sed -n 1p
echo "CFLAGS: $CFLAGS"
echo "CPPFLAGS: $CPPFLAGS"

mkdir -p "$OUT/obj"
cc1 "$OUT/src/linux/drivers/gpu/drm/drm_mm.c" "$OUT/obj/drm_mm.o"
cc1 "$OUT/src/linux/drivers/gpu/drm/drm_cache.c" "$OUT/obj/drm_cache.o"
cc1 "$HERE/drm_helpers/drm_helpers_mod.c" "$OUT/obj/drm_helpers_mod.o"
cc1 "$HERE/drm_mm/tests/drm_mm_selftest.c" "$OUT/obj/drm_mm_selftest.o"
cc1 "$HERE/drm_cache/tests/drm_cache_selftest.c" \
    "$OUT/obj/drm_cache_selftest.o"
cc1 "$HERE/test/selftest_kernel.c" "$OUT/obj/selftest_kernel.o"
cc1 "$HERE/test/drm_helpers_test.c" "$OUT/obj/drm_helpers_test.o"
if [ "${CROSS:-0}" = 1 ]; then
	echo "build.sh: compiled (CROSS=1: not linked)"
	exit 0
fi
: "${LKPI_MAPFILE:?set LKPI_MAPFILE to linuxkpi.mapfile from usr/src/uts/common/io/linuxkpi/build.sh}"

echo "\$ $LD $LDFLAGS -o $OUT/drm_helpers ..."
"$LD" $LDFLAGS -o "$OUT/drm_helpers" "$OUT/obj/drm_mm.o" "$OUT/obj/drm_cache.o" \
    "$OUT/obj/drm_helpers_mod.o"

sh "$HERE/gates.sh" "$OUT"

echo "\$ $LD $TEST_LDFLAGS -o $OUT/drm_helpers_test ..."
"$LD" $TEST_LDFLAGS -o "$OUT/drm_helpers_test" "$OUT/obj/drm_helpers_test.o" \
    "$OUT/obj/selftest_kernel.o" "$OUT/obj/drm_mm_selftest.o" \
    "$OUT/obj/drm_cache_selftest.o"

if command -v elfdump >/dev/null 2>&1; then
	for m in drm_helpers drm_helpers_test; do
		echo "\$ elfdump -d $OUT/$m | grep -E 'NEEDED|SUNW_KMOD'"
		elfdump -d "$OUT/$m" | grep -E 'NEEDED|SUNW_KMOD' || true
	done
fi
if command -v digest >/dev/null 2>&1; then
	digest -v -a sha256 "$OUT/drm_helpers" "$OUT/drm_helpers_test" || true
fi
echo "build.sh: built $OUT/drm_helpers and $OUT/drm_helpers_test"
