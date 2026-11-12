#!/bin/sh
# SPDX-License-Identifier: MIT
# Copyright 2026 The illumos-drm authors
# Stage only the helper files. Keeping this four-file include subset and
# the existing output paths preserves compiler flags and header selection.
set -eu
here=$(CDPATH= cd "$(dirname "$0")" && pwd)
repo=$(CDPATH= cd "$here/../../../../../.." && pwd)
LINUX=${LINUX:-$repo/usr/src/contrib/linux}
out=${1:?usage: prepare-sources.sh OUT}
[ -f "$LINUX/SOURCES" ] || { echo "missing $LINUX/SOURCES" >&2; exit 1; }
files='drivers/gpu/drm/drm_cache.c
drivers/gpu/drm/drm_mm.c
include/drm/drm_cache.h
include/drm/drm_mm.h'
printf '%s\n' "$files" |
    python3 "$repo/usr/src/uts/intel/io/i915n/check-licences.py" "$LINUX" /dev/fd/0
mkdir -p "$out/linux"
out=$(CDPATH= cd "$out" && pwd)
while IFS= read -r f; do
    mkdir -p "$out/linux/$(dirname "$f")"
    cp "$LINUX/$f" "$out/linux/$f"
done <<END_FILES
$files
END_FILES
# A reused output directory must not introduce other headers or symlinks.
actual=$(cd "$out/linux" && find . -type f -print | sed 's|^\./||' | LC_ALL=C sort)
links=$(cd "$out/linux" && find . -type l -print)
if [ "$actual" != "$files" ] || [ -n "$links" ]; then
    echo "unexpected files or symlinks in $out/linux" >&2
    exit 1
fi
echo "helpers: four vendored files staged in $out/linux; no fetching or patching"
