#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
# Rust module links formerly maintained inline in the native CI job.
set -eu
module=$1 archive=$2 output=$3
LD=${LD:-/usr/bin/ld}
run() {
    printf '%s\n' "$@" > "$output.argv"
    printf '%s ' "$@"; printf '\n'
    "$@"
}
case $module in
drm_core)
    run "$LD" -ztype=kmod -N fs/specfs -B reduce \
        -M "$CODEMGR_WS/usr/src/uts/common/io/drm/drm_core/drm_core.mapfile" \
        -z allextract "$archive" -z defaultextract "$4" -o "$output"
    ;;
virtio_gpu)
    run "$LD" -ztype=kmod -N misc/virtio -N fs/specfs -N misc/drm_core \
        -z allextract "$archive" -o "$output"
    ;;
rdrmnull)
    run "$LD" -ztype=kmod -N fs/specfs -N misc/drm_core \
        -z allextract "$archive" -o "$output"
    ;;
*) exit 2 ;;
esac
