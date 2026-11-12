#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
set -eu
HERE=$(CDPATH= cd "$(dirname "$0")" && pwd)
OUT=$1
SCOPE=$HERE/drm_helpers/drm_helpers_scope.mapfile
die() { echo "build.sh: $*" >&2; exit 1; }
# The module's global definitions are exactly the scope mapfile's.
nm "$OUT/drm_helpers" | awk -F'|' '$5 ~ /GLOB/ && $7 !~ /UNDEF/ {
    gsub(/ /, "", $8); print $8 }' | sort > "$OUT/drm_helpers.globals"
awk '/global:/ { g = 1; next } /local:/ { g = 0 }
    g && /;/ { gsub(/[;\t ]/, ""); print }' "$SCOPE" | sort \
    > "$OUT/drm_helpers.want"
diff "$OUT/drm_helpers.want" "$OUT/drm_helpers.globals" ||
    die "misc/drm_helpers' global symbols differ from $SCOPE"
# And the consumers' mapfile declares the same names, minus the entry points.
awk '/global:/ { g = 1; next } g && /EXTERN/ { print $1 }' \
    "$HERE/drm_helpers/drm_helpers.mapfile" | sort > "$OUT/drm_helpers.extern"
grep -v -e '^_init$' -e '^_fini$' -e '^_info$' "$OUT/drm_helpers.want" |
    diff - "$OUT/drm_helpers.extern" ||
    die "drm_helpers.mapfile and drm_helpers_scope.mapfile differ"
echo "misc/drm_helpers: $(wc -l < "$OUT/drm_helpers.globals" | tr -d ' ')" \
    "global symbols, as $(basename "$SCOPE")"

