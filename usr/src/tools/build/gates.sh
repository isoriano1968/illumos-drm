#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
# Link-time and explicit check gates. No module is loaded here.
set -eu
module=$1 phase=$2 OUT=$3
shift 3
repo=${CODEMGR_WS:?}
NM=${NM:-/usr/gnu/bin/nm}
LINUX=$repo/usr/src/contrib/linux
case $module in
linuxkpi|drm_helpers|drm_kms) HERE=$repo/usr/src/uts/common/io/$module ;;
i915n) HERE=$repo/usr/src/uts/intel/io/i915n ;;
*) HERE=$repo ;;
esac
die() { echo "gate: $*" >&2; exit 1; }
globals() {
    /usr/bin/nm "$1" | awk -F'|' '$5 ~ /GLOB/ && $7 !~ /UNDEF/ {gsub(/ /,"",$8); print $8}' | sort
}
case $phase:$module in
before:linuxkpi)
    sh "$HERE/gates.sh" private "$OUT"
    ;;
before:drm_kms|before:i915n)
    if [ "$#" = 0 ]; then die 'missing current object inventory'; fi
    objects=$(printf '%s\n' "$@" | sort)
    for o do cat "${o%.o}.d"; done | tr ' \\' '\n\n' |
        grep "^$LINUX/" | grep -v ':$' | sed "s|^$LINUX/||" | sort -u > "$OUT/linux-inputs.txt"
    [ -s "$OUT/linux-inputs.txt" ] || die 'no Linux dependencies'
    python3 "$repo/usr/src/uts/intel/io/i915n/check-licences.py" "$LINUX" "$OUT/linux-inputs.txt"
    if [ "$module" = drm_kms ]; then
        sh "$HERE/gates.sh" mapfiles "$OUT" "$@"
    else
        sh "$HERE/gen-aliases.sh" "$LINUX" > "$OUT/i915n.aliases"
        cmp "$OUT/i915n.aliases" "$HERE/i915n.aliases"
    fi
    ;;
after:linuxkpi)
    sh "$HERE/gates.sh" exports "$OUT"
    ;;
after:drm_helpers)
    sh "$HERE/gates.sh" "$OUT"
    ;;
after:drm_kms)
    sh "$HERE/gates.sh" globals "$OUT"
    ;;
after:drm_core)
    globals "$OUT/drm_core" > "$OUT/drm_core.globals"
    awk '/global:/ {g=1;next} /local:/ {g=0} g && /;/ {gsub(/[;\t ]/,"");print}' "$repo/usr/src/uts/common/io/drm/drm_core/drm_core.mapfile" | sort > "$OUT/drm_core.want"
    diff "$OUT/drm_core.want" "$OUT/drm_core.globals"
    elfdump -c "$OUT/drm_core" | grep SUNW_ctf
    ;;
after:virtio_gpu|after:rdrmnull)
    . "$repo/usr/src/test/drm-tests/drm-test-lib.sh"
    archive=${CARGO_TARGET_DIR:-$repo/target}/x86_64-illumos-kmod/release/drm_module.a
    n=$(core_symbols "$archive" | wc -l)
    [ "$n" -gt 0 ] || die 'core-symbol control failed'
    core_symbols "$OUT/$module" > "$OUT/$module.core"
    [ ! -s "$OUT/$module.core" ] || die "stateful core symbol in $module"
    ;;
esac
