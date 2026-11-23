#!/bin/bash
#
# SPDX-License-Identifier: MIT
# Copyright 2026 The OpenIndiana SPARC desktop contributors
#
# Builds drv/radeon: Linux v7.2's radeon (the TUs of tus.txt, from the
# vendored Linux tree), the carried TTM TUs and the illumos glue of
# illumos/, with linuxkpi's kernel flags (kflags.sh, LKPI_ARCH) and
# config.h, then links
#   ld -ztype=kmod -N misc/drm_core -N misc/linuxkpi -N misc/drm_helpers \
#      -N misc/drm_kms -z defs -M ddi.mapfile -M drm_facade.mapfile \
#      -M linuxkpi.mapfile -M drm_helpers.mapfile -M drm_kms.mapfile \
#      -M private.mapfile
# Modelled on usr/src/uts/common/io/drm_kms/build.sh; same environment
# (LINUX, REPO, CC, LD, SYSINC, DDI_MAPFILE, LKPI_MAPFILE, OUT, JOBS).
#
#   build.sh compile | link | all
set -u
PATH=/usr/gnu/bin:$PATH
export LC_ALL=C

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=${REPO:-$(cd "$HERE/../../../../../.." && pwd)}
LINUX=${LINUX:-$REPO/usr/src/contrib/linux}
CC=${CC:-gcc}
LD=${LD:-/usr/bin/ld}
SYSINC=${SYSINC:-/usr/include}
OUT=${OUT:-$PWD/out-radeon}
JOBS=${JOBS:-2}
CROSS=${CROSS:-0}
# GNU nm (BSD output format) for the export list: gnm on illumos.
if [ "$CROSS" = 1 ]; then
	NM=${NM:-x86_64-linux-gnu-nm}
else
	NM=${NM:-$(command -v gnm || echo /usr/gnu/bin/nm)}
fi
I915=$REPO/usr/src/uts/intel/io/i915n
mode=${1:-all}

die() {
	echo "radeon/build.sh: $*" >&2
	exit 1
}

[ -f "$LINUX/SOURCES" ] || die "$LINUX is not a vendored Linux tree (SOURCES missing)"
mkdir -p "$OUT/obj" "$OUT/log"
OUT=$(cd "$OUT" && pwd)
# shellcheck disable=SC1091
. "$REPO/usr/src/uts/common/io/linuxkpi/kflags.sh"
. "$HERE/flags.sh"
echo "$CC $CFLAGS $CPPFLAGS" > "$OUT/flags.txt"
"$CC" --version | sed -n 1p > "$OUT/compiler.txt"

# The TUs: Linux paths (tus.txt) and the illumos sources.
tus() {
	grep -v '^#' "$HERE/tus.txt" | grep .
	(cd "$HERE" && ls illumos/*.c 2>/dev/null)
}

compile_one() {
	tu=$1
	case "$tu" in
	illumos/*) src=$HERE/$tu ;;
	*) src=$LINUX/$tu ;;
	esac
	b=$(basename "${tu%.c}")
	obj=$OUT/obj/$b.o
	# shellcheck disable=SC2086
	if "$CC" $CFLAGS $CPPFLAGS "${BASENAME_CPPFLAG}\"$b\"" \
	    $DEPFLAGS "$OUT/obj/$b.d" -c "$src" -o "$obj" > "$OUT/log/$b.log" 2>&1; then
		echo "OK $tu"
	else
		rm -f "$obj"
		echo "FAIL $tu"
	fi
}
export -f compile_one
export CC CFLAGS CPPFLAGS OUT HERE LINUX BASENAME_CPPFLAG DEPFLAGS

if [ "$mode" = compile ] || [ "$mode" = all ]; then
	# The basenames must be unique: objects are named after them.
	dup=$(tus | xargs -n1 basename | sort | uniq -d)
	[ -z "$dup" ] || die "TUs with the same basename: $dup"
	# A new compile's logs and objects only; `link` keeps them.
	rm -rf "$OUT/obj" "$OUT/log"
	mkdir -p "$OUT/obj" "$OUT/log"
	tus | xargs -P "$JOBS" -I{} bash -c 'compile_one {}' > "$OUT/results.txt"
	ok=$(grep -c '^OK' "$OUT/results.txt")
	fail=$(grep -c '^FAIL' "$OUT/results.txt")
	{
		echo "compiled: $ok ok, $fail failed ($(wc -l < "$OUT/results.txt") TUs)"
		grep '^FAIL' "$OUT/results.txt"
		echo "--- missing headers (count)"
		grep -ho 'fatal error: [^:]*: No such file' "$OUT"/log/*.log |
		    sed 's/fatal error: //; s/: No such file//' | sort | uniq -c | sort -rn
		echo "--- errors (count, location: message)"
		grep -h 'error:' "$OUT"/log/*.log | sed "s|$OUT/sysinc|SYS|; s|$LINUX/||; s|$REPO/||" |
		    sort | uniq -c | sort -rn | head -80
	} > "$OUT/summary.txt"
	head -1 "$OUT/summary.txt"
	[ "$fail" -eq 0 ] || exit 1
	# Every file of the Linux tree the compiler read, for the licence
	# check and the package's licence file.
	cat "$OUT"/obj/*.d | tr ' \\' '\n\n' | grep "^$LINUX/" | grep -v ':$' |
	    sed "s|^$LINUX/||" | sort -u > "$OUT/linux-inputs.txt"
	echo "linux inputs: $(wc -l < "$OUT/linux-inputs.txt") files"
	[ -s "$OUT/linux-inputs.txt" ] ||
	    die "no Linux inputs found in the dependency files"
	python3 "$I915/check-licences.py" "$LINUX" "$OUT/linux-inputs.txt" || exit 1
fi


if [ "$mode" = link ] || [ "$mode" = all ]; then
	[ "$CROSS" = 1 ] && { echo "radeon/build.sh: CROSS=1, not linked"; exit 0; }
	: "${DDI_MAPFILE:?set DDI_MAPFILE to the ddi.mapfile of illumos-gate}"
	: "${LKPI_MAPFILE:?set LKPI_MAPFILE to linuxkpi.mapfile from usr/src/uts/common/io/linuxkpi/build.sh}"
	HELPERS_MAPFILE=${HELPERS_MAPFILE:-$REPO/usr/src/uts/common/io/drm_helpers/drm_helpers/drm_helpers.mapfile}
	FACADE_MAPFILE=${FACADE_MAPFILE:-$REPO/usr/src/uts/common/drm/drm_facade.mapfile}
	objs=$(ls "$OUT"/obj/*.o)
	echo "\$ $LD $LDFLAGS -o $OUT/radeon <$(echo "$objs" | wc -l) objects>" |
	    tee "$OUT/link.txt"
	# shellcheck disable=SC2086
	if "$LD" $LDFLAGS -o "$OUT/radeon" $objs >> "$OUT/link.txt" 2>&1; then
		echo "link: OK, 0 unresolved" | tee -a "$OUT/link.txt"
	else
		sed -n '/Undefined/,/symbol referencing errors/p' "$OUT/link.txt" | head -80
		echo "link: FAILED (see $OUT/link.txt)"
		exit 1
	fi
	if command -v elfdump >/dev/null 2>&1; then
		elfdump -d "$OUT/radeon" | grep -E 'NEEDED|SUNW_KMOD' || true
	fi
fi
