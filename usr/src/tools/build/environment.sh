#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
set -eu
: "${CODEMGR_WS:?source myenv.sh from the workspace root}"
printf 'illumos-drm build: '; date -u
uname -a
for n in CODEMGR_WS ROOT DMAKE_MAX_JOBS CARGO_BUILD_JOBS CC LD CARGO RUSTC \
    RUST_TOOLCHAIN RUSTC_BOOTSTRAP RUSTFLAGS CARGO_FLAGS CARGO_HOME CARGO_TARGET_DIR \
    CTFCONVERT SYSINC DDI_MAPFILE PKGREPO; do
    v=$(printenv "$n" || :)
    printf '%s=%s\n' "$n" "$v"
done
"${CC:-gcc}" --version | sed -n 1p
if [ "${RUST_TOOLCHAIN:-}" = system ]; then
    "${CARGO:-cargo}" --version
else
    "${CARGO:-cargo}" +"${RUST_TOOLCHAIN:-nightly-2026-04-16}" --version
fi
if [ "${RUST_TOOLCHAIN:-}" = system ]; then
    "${RUSTC:-rustc}" -Vv
else
    rustup run "${RUST_TOOLCHAIN:-nightly-2026-04-16}" rustc -Vv
fi
