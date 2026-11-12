# SPDX-License-Identifier: MIT
# Copyright 2026 The illumos-drm authors
# Sourced by the independent builder and the make flag generator.
CFLAGS="-m64 -std=gnu99 -O0 -gdwarf-4 -fno-eliminate-unused-debug-types"
CFLAGS="$CFLAGS -ffreestanding -mcmodel=kernel -mno-red-zone -fno-builtin"
CPPFLAGS="-D_KERNEL -D_SYSCALL32 -D_ELF64 -D_DDI_STRICT -Dsun -D__sun -D__SVR4"
CPPFLAGS="$CPPFLAGS -nostdinc -I$REPO/usr/src/uts/common -I$SYSINC"
FACADE_CHECK_FLAGS="-m64 -D_KERNEL -std=gnu99 -I$REPO/usr/src/uts/common"
