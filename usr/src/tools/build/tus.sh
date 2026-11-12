#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
# Print source paths, using the committed TU lists as the only inventory.
set -eu
repo=${CODEMGR_WS:?}
here=$repo/usr/src/uts/intel/io/i915n
list() {
case $1 in
kms)
    here=$repo/usr/src/uts/common/io/drm_kms
    grep -v '^#' "$here/tus.txt" | grep .
    (cd "$here" && ls illumos/*.c)
    ;;
i915)
    awk -v display="${I915_DISPLAY:-1}" '
    FNR == NR {if ($0 !~ /^(#|$)/) {base[++n]=$0; present[$0]=1}; next}
    display == 1 && $0 !~ /^(#|$)/ {
        if ($0 ~ /^-/) {t=substr($0,2); if (!(t in present)) exit 1; removed[t]=1}
        else extra[++m]=$0
    }
    END {for (i=1;i<=n;i++) if (!(base[i] in removed)) print base[i]
         for (i=1;i<=m;i++) print extra[i]}' "$here/tus.txt" "$here/display-tus.txt"
    ;;
*) exit 2 ;;
esac
}
entries=$(list "$1")
printf '%s\n' "$entries" | awk -v kind="$1" '
{ key=$0; if (kind == "kms") sub(/^.*\//,"",key)
  if (seen[key]++) {print "duplicate TU/object: " key > "/dev/stderr"; bad=1}
  print }
END {exit bad}'
