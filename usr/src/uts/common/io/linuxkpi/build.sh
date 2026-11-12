#!/bin/bash
#
# SPDX-License-Identifier: MIT
# Copyright 2026 The illumos-drm authors
#
# Builds the LinuxKPI layer: compiles usr/src/uts/common/io/linuxkpi/src/*.c with the kernel
# flags of usr/src/uts/common/io/linuxkpi/kflags.sh and links the kernel module misc/linuxkpi
#   ld -ztype=kmod -N misc/drm_core -z defs -M ddi.mapfile \
#      -M usr/src/uts/common/drm/drm_facade.mapfile -M private.mapfile ...
# where private.mapfile is generated from private.d/*.txt: the kernel
# symbols the layer uses that ddi.mapfile lacks, one per line with the
# reason (documented 9F, or a non-DDI interface and why it is needed).
# and writes linuxkpi.mapfile, the EXTERN declarations of the module's
# global symbols for the drivers that link against it.
#
#   usr/src/uts/common/io/linuxkpi/build.sh compile | link | selftest | all
#
# selftest builds the test driver linuxkpi_selftest (selftest/), linked
#   ld -ztype=kmod -N misc/linuxkpi -N misc/drm_core -z defs -M ddi.mapfile \
#      -M drm_facade.mapfile -M linuxkpi.mapfile ...
# i.e. against the layer's exported symbols, as i915n is.
#
# Environment: CC [gcc], LD [/usr/bin/ld], SYSINC [/usr/include],
# DDI_MAPFILE (the gate's ddi.mapfile), OUT [./out-linuxkpi], JOBS [2],
# CROSS [0] (see kflags.sh), SRCS (only these src/ files, for development),
# FACADE_MAPFILE [usr/src/uts/common/drm/drm_facade.mapfile].
set -u
export LC_ALL=C
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=${REPO:-$(cd "$HERE/../../../../../.." && pwd)}
CC=${CC:-gcc}
LD=${LD:-/usr/bin/ld}
SYSINC=${SYSINC:-/usr/include}
OUT=${OUT:-$PWD/out-linuxkpi}
JOBS=${JOBS:-2}
CROSS=${CROSS:-0}
mode=${1:-all}
FACADE_MAPFILE=${FACADE_MAPFILE:-$REPO/usr/src/uts/common/drm/drm_facade.mapfile}

rm -rf "$OUT/log"
mkdir -p "$OUT/obj" "$OUT/log"
# shellcheck disable=SC1091
. "$HERE/kflags.sh"
. "$HERE/flags.sh"
echo "$CC $CFLAGS $CPPFLAGS" > "$OUT/flags.txt"

compile_one() {
	src=$1
	b=$(basename "${src%.c}")
	if "$CC" $CFLAGS $CPPFLAGS -c "$src" -o "$OUT/obj/$b.o" > "$OUT/log/$b.log" 2>&1; then
		echo "OK $b"
	else
		rm -f "$OUT/obj/$b.o"
		echo "FAIL $b"
	fi
}
export -f compile_one
export CC CFLAGS CPPFLAGS OUT

if [ "$mode" = compile ] || [ "$mode" = all ]; then
	rm -f "$OUT"/obj/*.o
	# SRCS: a space-separated subset of src/*.c to compile (development).
	if [ -n "${SRCS:-}" ]; then
		for f in $SRCS; do echo "$HERE/src/$f"; done
	else
		ls "$HERE"/src/*.c
	fi | xargs -P "$JOBS" -I{} bash -c 'compile_one {}' > "$OUT/results.txt"
	sort "$OUT/results.txt" | grep FAIL
	echo "linuxkpi: $(grep -c '^OK' "$OUT/results.txt") compiled, $(grep -c '^FAIL' "$OUT/results.txt") failed"
	grep -h 'error:' "$OUT"/log/*.log | head -40
	grep -q '^FAIL' "$OUT/results.txt" && exit 1
fi

if [ "$mode" = link ] || [ "$mode" = all ]; then
	: "${DDI_MAPFILE:?set DDI_MAPFILE to the ddi.mapfile of illumos-gate}"
	sh "$HERE/gates.sh" private "$OUT"
	"$LD" $LDFLAGS -o "$OUT/linuxkpi" "$OUT"/obj/*.o || exit 1
	sh "$HERE/gates.sh" exports "$OUT"
	echo "linuxkpi: linked $OUT/linuxkpi; $(grep -c EXTERN "$OUT/linuxkpi.mapfile") exported symbols"
fi

if [ "$mode" = selftest ] || [ "$mode" = all ]; then
	: "${DDI_MAPFILE:?set DDI_MAPFILE to the ddi.mapfile of illumos-gate}"
	b=linuxkpi_selftest
	mkdir -p "$OUT/selftest"
	rm -f "$OUT"/selftest/*.o
	for f in "$HERE"/selftest/*.c; do
		o=$OUT/selftest/$(basename "${f%.c}").o
		# shellcheck disable=SC2086
		"$CC" $CFLAGS $STCPP -c "$f" -o "$o" \
		    > "$OUT/log/$b.log" 2>&1 || { cat "$OUT/log/$b.log"; exit 1; }
	done
	[ "$mode" = selftest ] && [ ! -f "$OUT/linuxkpi.mapfile" ] &&
	    { echo "linuxkpi: link the layer first"; exit 1; }
	"$LD" $STLDFLAGS -o "$OUT/$b" "$OUT"/selftest/*.o || exit 1
	echo "linuxkpi: linked $OUT/$b"
fi
