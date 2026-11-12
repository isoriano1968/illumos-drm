# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
# Source from the workspace root before invoking /usr/bin/dmake.
CODEMGR_WS=${CODEMGR_WS:-$(pwd)}
ROOT=${ROOT:-$CODEMGR_WS/proto/root_i386}
DMAKE_MAX_JOBS=${DMAKE_MAX_JOBS:-2}
CARGO_BUILD_JOBS=${CARGO_BUILD_JOBS:-2}
CC=${CC:-gcc}
LD=${LD:-/usr/bin/ld}
CARGO=${CARGO:-cargo}
RUST_TOOLCHAIN=${RUST_TOOLCHAIN:-nightly-2026-04-16}
case $RUST_TOOLCHAIN in
system) RUSTFLAGS='-C symbol-mangling-version=v0 -C force-frame-pointers=yes' ;;
*) RUSTFLAGS=${RUSTFLAGS-} ;;
esac
export RUSTFLAGS
CARGO_FLAGS=${CARGO_FLAGS:---locked}
CTFCONVERT=${CTFCONVERT:-/opt/onbld/bin/i386/ctfconvert}
SYSINC=${SYSINC:-/usr/include}
DDI_MAPFILE=${DDI_MAPFILE:-$CODEMGR_WS/target/ddi.mapfile}
CARGO_TARGET_DIR=${CARGO_TARGET_DIR:-$CODEMGR_WS/target}
PKGREPO=${PKGREPO:-$CODEMGR_WS/packages/i386/nightly/repo.redist}
export CODEMGR_WS ROOT DMAKE_MAX_JOBS CARGO_BUILD_JOBS CC LD CARGO
export RUST_TOOLCHAIN CARGO_FLAGS CTFCONVERT SYSINC DDI_MAPFILE
export CARGO_TARGET_DIR PKGREPO
LC_ALL=C
export LC_ALL
