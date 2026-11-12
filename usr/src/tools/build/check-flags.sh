#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
# Check the identity parsed by make, never interpret a shell program.
set -eu
identity=yes
if [ "$1" = --current ]; then
    identity=no
    config=$2 parsed=
else
    config=$1 parsed=$2
fi
shift 2
stale() {
    echo "flags include $config is missing, stale, or changed since make parsed it" >&2
    echo 'run /usr/bin/dmake flags && /usr/bin/dmake all in this module directory' >&2
    exit 1
}
[ -f "$config" ] || stale
if [ "$identity" = yes ]; then
    [ -n "$parsed" ] || stale
    grep -F -x "FLAGS_TOKEN = $parsed" "$config" >/dev/null || stale
fi
for input do
    [ ! "$input" -nt "$config" ] || stale
done
