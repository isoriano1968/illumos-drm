# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 The illumos-drm authors
# Sourced by the independent builder and the make flag generator.
TMPD=$(mktemp -d /tmp/cdrmnull-build.XXXXXX)
trap 'rm -rf "$TMPD"' EXIT
trap 'exit 1' INT TERM
printf 'int cdrmnull_probe;\n' > "$TMPD/probe.c"

# Whether the compiler takes these flags without a warning.
probe() {
	"$CC" -m64 -Werror "$@" -c -o "$TMPD/probe.o" "$TMPD/probe.c" \
	    >/dev/null 2>&1
}

# The gate's flags for a 64-bit kernel module, as cw passes them to gcc.
CFLAGS="-m64 -mtune=opteron -Ui386 -U__i386 -O2 -std=gnu99"
CFLAGS="$CFLAGS -fno-strict-aliasing -fno-optimize-sibling-calls"
CFLAGS="$CFLAGS -ffreestanding -mcmodel=kernel -mno-red-zone"
CFLAGS="$CFLAGS -mno-mmx -mno-sse"
CFLAGS="$CFLAGS -fident -finline -fno-inline-functions -fno-builtin"
CFLAGS="$CFLAGS -fno-asm -fdiagnostics-show-option"
CFLAGS="$CFLAGS -fno-asynchronous-unwind-tables"
CFLAGS="$CFLAGS -Wall -Wextra -Werror -Wno-missing-braces -Wno-sign-compare"
CFLAGS="$CFLAGS -Wno-unused-parameter -Wno-missing-field-initializers"
# shellcheck disable=SC2086 # word splitting of the flag list
probe $CFLAGS || die "$CC does not take the base kernel flags: $CFLAGS"

# Flags of the gate that depend on the compiler's version or on illumos'
# gcc patches (-msave-args): used where the compiler has them.
for f in -msave-args \
    "-mindirect-branch=thunk-extern -mindirect-branch-register" \
    -fno-shrink-wrap -fno-unit-at-a-time \
    -fno-inline-small-functions -fno-inline-functions-called-once \
    -fno-ipa-cp -fno-ipa-icf -fno-clone-functions \
    -fno-reorder-functions -fno-reorder-blocks-and-partition \
    -fno-aggressive-loop-optimizations \
    --param=max-inline-insns-single=450 \
    -fstack-protector-strong; do
	# shellcheck disable=SC2086 # $f may hold two flags
	if probe $f; then
		CFLAGS="$CFLAGS $f"
	else
		echo "build.sh: $CC does not take $f; built without it"
	fi
done

CPPFLAGS="-D_KERNEL -D_SYSCALL32 -D_SYSCALL32_IMPL -D_ELF64 -D_DDI_STRICT"
CPPFLAGS="$CPPFLAGS -Dsun -D__sun -D__SVR4 -D_ASM_INLINES"
# Only the facade and the system headers: the facade's drm/ directory
# first (it has no drm.h), then libdrm's drm/drm.h and the kernel headers.
CPPFLAGS="$CPPFLAGS -nostdinc -I$REPO/usr/src/uts/common -I$SYSINC"
LDFLAGS="-ztype=kmod -N misc/drm_core -z defs -M $DDI_MAPFILE -M $FACADE_MAPFILE -M $PRIVATE_MAPFILE"
