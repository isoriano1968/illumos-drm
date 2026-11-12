#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
set -eu
[ "$(uname -s)" = SunOS ] || { echo 'use the system dmake on illumos' >&2; exit 1; }
for t in /usr/bin/dmake "${CC:-gcc}" "${LD:-/usr/bin/ld}" "${CARGO:-cargo}" \
    "${CTFCONVERT:-/opt/onbld/bin/i386/ctfconvert}" /usr/gnu/bin/ln pkgsend pkgrepo pkglint; do
    command -v "$t" >/dev/null 2>&1 || { echo "setup: missing required tool $t" >&2; exit 1; }
done
: "${DDI_MAPFILE:?set DDI_MAPFILE to the gate ddi.mapfile}"
[ -f "$DDI_MAPFILE" ]
[ -f "${SYSINC:-/usr/include}/drm/drm.h" ]
case ${CODEMGR_WS:?} in *' '*|*'\t'*) echo 'workspace path must not contain whitespace' >&2; exit 1 ;; esac
