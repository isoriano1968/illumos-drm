# SPDX-License-Identifier: MIT
# Copyright 2026 The illumos-drm authors
#
# The compiler flags of everything built against the LinuxKPI layer (the
# layer itself and the Linux drivers): the illumos-gate's 64-bit kernel
# flags as usr/src/uts/common/io/cdrmnull/build.sh uses them, plus the Linux dialect
# (gnu11, -fms-extensions, unsigned char, no strict aliasing/overflow).
# Sourced by usr/src/uts/common/io/linuxkpi/build.sh and usr/src/uts/intel/io/i915n/build.sh with CC, CROSS,
# SYSINC, REPO and OUT set; sets LKPI_CFLAGS and LKPI_CPPFLAGS.
#
#   CROSS=1   CC is a Linux-hosted x86_64 cross gcc (development only):
#             drop illumos-only flags, undefine the Linux target macros.
#   LKPI_ARCH=aarch64
#             CC is an aarch64-unknown-solaris2.11 gcc and GATE an
#             illumos-gate with the aarch64 port; its uts headers replace
#             SYSINC, searched after the layer's own.

LKPI_ARCH=${LKPI_ARCH:-x86_64}
case $LKPI_ARCH in
aarch64)
	: "${GATE:?set GATE to the illumos-gate with the aarch64 port}"
	SYSINC=/nonexistent
	_lkpi_m64=
	;;
sparcv9)
	: "${GATE:?set GATE to the illumos-gate the SPARC kernel was built from}"
	_lkpi_m64=-m64
	;;
x86_64)
	_lkpi_m64=-m64
	;;
*)
	echo "kflags.sh: unknown LKPI_ARCH $LKPI_ARCH" >&2
	exit 1
	;;
esac
mkdir -p "$OUT/sysinc"
# Only the kernel header directories of the system include tree, so a
# Linux header the layer lacks is an error and not a userland header.
for d in sys vm asm ia32 amd64 netinet inet net rpc; do
	# rm first: illumos ln -sfn onto an existing link to a directory
	# creates the new link inside that directory.
	[ -e "$SYSINC/$d" ] && { rm -f "$OUT/sysinc/$d"; ln -s "$SYSINC/$d" "$OUT/sysinc/$d"; }
done

_lkpi_tmp=$(mktemp -d /tmp/lkpi-flags.XXXXXX)
printf 'int probe;\n' > "$_lkpi_tmp/probe.c"
_lkpi_probe() {
	"$CC" $_lkpi_m64 -Werror "$@" -c -o "$_lkpi_tmp/probe.o" "$_lkpi_tmp/probe.c" >/dev/null 2>&1
}

# The flags a kernel module cannot do without: a compiler that refuses
# one of them cannot build the layer (checked below, not dropped).
_lkpi_must="$_lkpi_m64 -O2 -std=gnu11 -fno-strict-aliasing -fno-strict-overflow"
_lkpi_must="$_lkpi_must -ffreestanding -fno-pie -fno-pic"
if [ "$LKPI_ARCH" = aarch64 ]; then
	# No FP registers; aligned accesses only (Device memory faults on
	# others); no outlined atomics (they are libgcc's); krtld knows
	# neither GOT nor descriptor TLS.
	_lkpi_must="$_lkpi_must -mgeneral-regs-only -mstrict-align -mcmodel=large"
	_lkpi_must="$_lkpi_must -mno-outline-atomics -mtls-dialect=trad"
	_lkpi_must="$_lkpi_must -fno-omit-frame-pointer"
elif [ "$LKPI_ARCH" = sparcv9 ]; then
	# The gate's sparcv9 kernel flags: abs32 code (-xcode=abs32), no FP
	# registers (-xregs=no%float), %g2-%g4 left to applications
	# (-xregs=no%appl).
	_lkpi_must="$_lkpi_must -mcmodel=medlow -mno-fpu -mno-app-regs"
	_lkpi_must="$_lkpi_must -fno-omit-frame-pointer"
else
	_lkpi_must="$_lkpi_must -mcmodel=kernel -mno-red-zone"
	_lkpi_must="$_lkpi_must -mno-mmx -mno-sse -mno-sse2 -mno-avx -mno-80387 -mno-fp-ret-in-387"
fi
LKPI_CFLAGS="$_lkpi_must -fno-optimize-sibling-calls"
LKPI_CFLAGS="$LKPI_CFLAGS -fno-builtin -fno-common -fno-asynchronous-unwind-tables"
LKPI_CFLAGS="$LKPI_CFLAGS -fms-extensions -funsigned-char -fno-delete-null-pointer-checks"
LKPI_CFLAGS="$LKPI_CFLAGS -fno-jump-tables -fdiagnostics-show-option"
LKPI_CFLAGS="$LKPI_CFLAGS -Wall -Wno-unused-parameter -Wno-sign-compare"
LKPI_CFLAGS="$LKPI_CFLAGS -Wno-missing-field-initializers -Wno-unused-function"
LKPI_CFLAGS="$LKPI_CFLAGS -Wno-address-of-packed-member -Wno-format-truncation"
LKPI_CFLAGS="$LKPI_CFLAGS -Wno-microsoft-anon-tag -Wno-override-init -Wno-comment"
# Linux builds with -Wno-pointer-sign too (char * vs u8 * throughout).
LKPI_CFLAGS="$LKPI_CFLAGS -Wno-pointer-sign"
LKPI_CFLAGS="$LKPI_CFLAGS -Werror=implicit-function-declaration -Werror=implicit-int"
LKPI_CFLAGS="$LKPI_CFLAGS -Werror=incompatible-pointer-types -Werror=int-conversion"
LKPI_CFLAGS="$LKPI_CFLAGS -Werror=return-type"
# The gate's flags for DTrace fbt and stack traces (as cw passes them),
# where the compiler takes them.
for _f in -msave-args "-mindirect-branch=thunk-extern -mindirect-branch-register" \
    -fno-shrink-wrap -fno-ipa-cp -fno-ipa-icf -fno-clone-functions \
    -fno-reorder-functions -fno-reorder-blocks-and-partition \
    -fno-inline-small-functions -fno-inline-functions-called-once \
    -fno-aggressive-loop-optimizations -fstack-protector-strong; do
	if [ "${CROSS:-0}" = 1 ] && [ "$_f" = -msave-args ]; then continue; fi
	# illumos builds its sparc kernel without the stack protector
	# (no __stack_chk_guard there).
	if [ "${LKPI_ARCH:-}" = sparcv9 ] && [ "$_f" = -fstack-protector-strong ]; then continue; fi
	# shellcheck disable=SC2086
	_lkpi_probe $_f && LKPI_CFLAGS="$LKPI_CFLAGS $_f"
done
# shellcheck disable=SC2086
if ! _lkpi_probe $_lkpi_must; then
	echo "kflags.sh: $CC does not take the kernel flags $_lkpi_must" >&2
	rm -rf "$_lkpi_tmp"
	exit 1
fi
# The warning and dialect flags: kept where the compiler takes them
# (each probed with the kernel flags, which some depend on).
# shellcheck disable=SC2086
LKPI_CFLAGS=$(for _f in $LKPI_CFLAGS; do _lkpi_probe $_lkpi_must "$_f" && printf '%s ' "$_f"; done)
rm -rf "$_lkpi_tmp"

if [ "$LKPI_ARCH" = aarch64 ]; then
	_lkpi_uts=$GATE/usr/src/uts
	LKPI_CPPFLAGS="-D_KERNEL -D_ELF64 -D_DDI_STRICT -D_MACHDEP -Darmv8 -Daarch64"
	for d in "$_lkpi_uts/armv8" "$_lkpi_uts/aarch64" "$GATE/usr/src/common" "$_lkpi_uts/common"; do
		LKPI_CPPFLAGS="$LKPI_CPPFLAGS -idirafter $d"
	done
elif [ "$LKPI_ARCH" = sparcv9 ]; then
	_lkpi_uts=$GATE/usr/src/uts
	LKPI_CPPFLAGS="-D_KERNEL -D_SYSCALL32 -D_SYSCALL32_IMPL -D_ELF64 -D_DDI_STRICT"
	LKPI_CPPFLAGS="$LKPI_CPPFLAGS -D_MACHDEP -Dsun4u -D__sparcv9cpu"
	for d in "$_lkpi_uts/sun4u" "$_lkpi_uts/sfmmu" "$_lkpi_uts/sun4" "$_lkpi_uts/sparc/v9" "$_lkpi_uts/sparc" "$GATE/usr/src/common" "$_lkpi_uts/common"; do
		LKPI_CPPFLAGS="$LKPI_CPPFLAGS -idirafter $d"
	done
else
	LKPI_CPPFLAGS="-D_KERNEL -D_SYSCALL32 -D_SYSCALL32_IMPL -D_ELF64 -D_DDI_STRICT"
fi
LKPI_CPPFLAGS="$LKPI_CPPFLAGS -Dsun -D__sun -D__SVR4 -D__illumos__ -D_ASM_INLINES"
LKPI_CPPFLAGS="$LKPI_CPPFLAGS -D__KERNEL__ -DMODULE"
if [ "${CROSS:-0}" = 1 ]; then
	LKPI_CPPFLAGS="$LKPI_CPPFLAGS -U__linux__ -U__linux -Ulinux -U__gnu_linux__ -U__unix__ -Uunix"
fi
