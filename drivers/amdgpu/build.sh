#!/bin/bash
#
# SPDX-License-Identifier: MIT
# Copyright 2026 The illumos-drm authors
#
# Builds amdgpu (Southern Islands first) from the pinned Linux tree
# against the DRM facade and the LinuxKPI layer, like drivers/i915/build.sh:
#
#   drivers/amdgpu/build.sh compile   compile every TU of tus.txt (-k style)
#   drivers/amdgpu/build.sh link      link "amdgpu" with -z defs and report
#                                     the unresolved symbols
#   drivers/amdgpu/build.sh all       both
#
# tus.txt (gen-tus.sh): amdgpu-relative paths, "/"-prefixed tree paths
# (TTM, scheduler, DRM helpers carried until they get modules of their
# own), illumos/*.c (this directory, MIT).
#
# Environment: LINUX (the tree of fetch-and-patch.sh, required), REPO,
# CC [gcc], LD [/usr/bin/ld], SYSINC [/usr/include], OUT [./out-amdgpu],
# JOBS [2], DDI_MAPFILE, LKPI_MAPFILE (link only).
set -u
PATH=/usr/gnu/bin:$PATH
export LC_ALL=C

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=${REPO:-$(cd "$HERE/../.." && pwd)}
: "${LINUX:?set LINUX to the patched Linux tree}"
CC=${CC:-gcc}
LD=${LD:-/usr/bin/ld}
SYSINC=${SYSINC:-/usr/include}
OUT=${OUT:-$PWD/out-amdgpu}
JOBS=${JOBS:-2}
AMDDIR=$LINUX/drivers/gpu/drm/amd
mode=${1:-all}

mkdir -p "$OUT/obj" "$OUT/log"
# shellcheck disable=SC1091
. "$REPO/linuxkpi/kflags.sh"
CFLAGS=$LKPI_CFLAGS
CPPFLAGS="$LKPI_CPPFLAGS -DKBUILD_MODNAME=\"amdgpu\""
CPPFLAGS="$CPPFLAGS -nostdinc -I$REPO/linuxkpi/include -I$REPO/include"
CPPFLAGS="$CPPFLAGS -I$LINUX/.illumos/mit-include -I$LINUX/.illumos/mit-include/uapi"
# amdgpu's own include directories (its Makefiles' ccflags)
for d in include/asic_reg include amdgpu pm/inc pm/swsmu pm/swsmu/inc \
    pm/swsmu/inc/pmfw_if pm/swsmu/smu11 pm/swsmu/smu12 pm/swsmu/smu13 \
    pm/swsmu/smu14 pm/swsmu/smu15 pm/powerplay/inc pm/powerplay/smumgr \
    pm/powerplay/hwmgr pm/legacy-dpm acp/include display display/include \
    display/modules/inc display/dc display/amdgpu_dm amdkfd ras/ras_mgr \
    ras/rascore; do
	CPPFLAGS="$CPPFLAGS -I$AMDDIR/$d"
done
CPPFLAGS="$CPPFLAGS -I$OUT/sysinc -I$HERE/illumos"
CPPFLAGS="$CPPFLAGS -include $HERE/config.h -include $REPO/linuxkpi/include/linux/kconfig.h"
CPPFLAGS="$CPPFLAGS -include $REPO/linuxkpi/include/lkpi/prologue.h"
# __LITTLE_ENDIAN everywhere, as Linux's asm/byteorder.h gives it: the DC
# headers pick their bitfield layouts from it before any byte-order header.
CPPFLAGS="$CPPFLAGS -include $REPO/linuxkpi/include/lkpi/byteorder.h"
CFLAGS="$CFLAGS -Wno-override-init"
# Linux's default (W=0) GCC warning set for its own files
# (scripts/Makefile.warn): these are off in every Linux build. Our glue
# (illumos/*) keeps them.
LINUX_WFLAGS="-Wno-maybe-uninitialized -Wno-unused-but-set-variable"
LINUX_WFLAGS="$LINUX_WFLAGS -Wno-unused-const-variable -Wno-dangling-pointer"
LINUX_WFLAGS="$LINUX_WFLAGS -Wno-array-bounds"

grep -v '^#' "$HERE/tus.txt" | grep . > "$OUT/tus.txt"
[ -z "$(sort "$OUT/tus.txt" | uniq -d)" ] ||
    { echo "build.sh: a TU is listed twice" >&2; exit 1; }
echo "$CC $CFLAGS $CPPFLAGS" > "$OUT/flags.txt"

compile_one() {
	tu=$1
	case "$tu" in
	illumos/*) src=$HERE/$tu; name=$tu; wflags= ;;
	/*) src=$LINUX$tu; name=${tu#/}; wflags=$LINUX_WFLAGS ;;
	*) src=$AMDDIR/amdgpu/$tu; name=drivers/gpu/drm/amd/amdgpu/$tu
	    wflags=$LINUX_WFLAGS ;;
	esac
	name=$(echo "$name" | sed 's|/[^/]*/\.\./|/|')
	obj=$OUT/obj/${name%.c}.o
	log=$OUT/log/$(echo "${name%.c}" | tr / _).log
	mkdir -p "$(dirname "$obj")"
	# The carried Linux modules' own entry points (<linux/module.h>);
	# the illumos glue calls them before amdgpu's.
	case "$tu" in
	/drivers/gpu/buddy.c) ent=gpu_buddy ;;
	/drivers/gpu/drm/scheduler/sched_fence.c) ent=drm_sched ;;
	*) ent= ;;
	esac
	defs=
	[ -z "$ent" ] ||
	    defs="-DLKPI_MODULE_INIT=lkpi_module_init_$ent -DLKPI_MODULE_EXIT=lkpi_module_exit_$ent"
	# shellcheck disable=SC2086
	if "$CC" $CFLAGS $wflags $CPPFLAGS $defs -DKBUILD_BASENAME=\"$(basename "${tu%.c}")\" \
	    -MD -MF "${obj%.o}.d" -c "$src" -o "$obj" > "$log" 2>&1; then
		echo "OK $name"
	else
		rm -f "$obj"
		echo "FAIL $name"
	fi
}
export -f compile_one
export CC CFLAGS LINUX_WFLAGS CPPFLAGS OUT AMDDIR HERE LINUX

if [ "$mode" = compile ] || [ "$mode" = all ]; then
	rm -rf "$OUT/obj" "$OUT/log"
	mkdir -p "$OUT/log"
	: > "$OUT/results.txt"
	xargs -P "$JOBS" -I{} bash -c 'compile_one {}' < "$OUT/tus.txt" \
	    >> "$OUT/results.txt"
	ok=$(grep -c '^OK' "$OUT/results.txt")
	fail=$(grep -c '^FAIL' "$OUT/results.txt")
	{
		echo "compiled: $ok ok, $fail failed ($(wc -l < "$OUT/results.txt") TUs)"
		echo "warnings: $(cat "$OUT"/log/*.log | grep -c 'warning:')" \
		    "($(cat "$OUT"/log/*.log | grep 'warning:' | sort -u | wc -l) different)"
		echo "--- missing headers (count)"
		grep -ho 'fatal error: [^:]*: No such file' "$OUT"/log/*.log |
		    sed 's/fatal error: //; s/: No such file//' | sort | uniq -c | sort -rn
		echo "--- first errors (count, location: message)"
		for l in "$OUT"/log/*.log; do
			grep -m1 -E 'error:' "$l" | sed "s|$OUT/sysinc|SYS|; s|$LINUX/||; s|$REPO/||"
		done | sort | uniq -c | sort -rn | head -100
		echo "--- warning kinds (count of different warnings)"
		cat "$OUT"/log/*.log | grep 'warning:' | sort -u |
		    grep -oE '\[-W[^]]+\]|warning: #warning|redefined' |
		    sort | uniq -c | sort -rn
		echo "--- error kinds (count)"
		grep -h 'error:' "$OUT"/log/*.log | sed -E 's/^[^ ]+ error: //' |
		    sed -E "s/'[^']*'/X/g" | sort | uniq -c | sort -rn | head -60
	} > "$OUT/summary.txt"
	head -2 "$OUT/summary.txt"
	find "$OUT/obj" -name '*.d' -exec cat {} + 2>/dev/null |
	    tr ' \\' '\n\n' | grep "^$LINUX/" | grep -v ':$' |
	    sed "s|^$LINUX/||" | sed 's|/[^/]*/\.\./|/|g' | sort -u > "$OUT/linux-inputs.txt"
	echo "linux inputs: $(wc -l < "$OUT/linux-inputs.txt") files"
	if [ -s "$OUT/linux-inputs.txt" ]; then
		python3 "$REPO/drivers/i915/check-licences.py" "$LINUX" "$OUT/linux-inputs.txt" ||
		    echo "build.sh: LICENCE CHECK REFUSED files (see above)"
	fi
	[ "$fail" -eq 0 ] || exit 1
fi

if [ "$mode" = link ] || [ "$mode" = all ]; then
	: "${DDI_MAPFILE:?set DDI_MAPFILE}" "${LKPI_MAPFILE:?set LKPI_MAPFILE}"
	HELPERS_MAPFILE=${HELPERS_MAPFILE:-$REPO/carried/drm_helpers/drm_helpers.mapfile}
	KMS_MAPFILE=${KMS_MAPFILE:-$REPO/carried/drm_kms/drm_kms.mapfile}
	objs=$(find "$OUT/obj" -name '*.o' | sort)
	# shellcheck disable=SC2086
	if "$LD" -ztype=kmod -N misc/drm_core -N misc/linuxkpi -N misc/drm_helpers \
	    -N misc/drm_kms -z defs -M "$DDI_MAPFILE" -M "$REPO/include/drm/drm_facade.mapfile" \
	    -M "$LKPI_MAPFILE" -M "$HELPERS_MAPFILE" -M "$KMS_MAPFILE" \
	    -M "$HERE/private.mapfile" \
	    -o "$OUT/amdgpu" $objs > "$OUT/link.txt" 2>&1; then
		echo "link: OK, 0 unresolved"
	else
		awk '/^Undefined/{f=1;next} /symbol referencing errors/{f=0} f{print $1}' \
		    "$OUT/link.txt" | sort -u > "$OUT/unresolved.txt"
		echo "link: $(wc -l < "$OUT/unresolved.txt") unresolved symbols (unresolved.txt)"
		exit 1
	fi
fi
