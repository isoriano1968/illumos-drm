#!/bin/sh
# SPDX-License-Identifier: MIT
# Copyright 2026 The illumos-drm authors
#
# Exercise the actual DIRTYFB flush-range function of i915n.
#
# i915n_dirty_ranges(), with its clip bound and range type, is cut out of
# the real i915_illumos.c and compiled with stand-ins for the kernel's
# u64 and struct drm_clip_rect, then checked for no clips, one clip,
# out-of-bounds and inverted clips, the clip-count and row bounds, and
# the offset/pitch arithmetic at the end of the object. Everything lives
# in one mktemp -d directory, removed at exit.

set -eu
LC_ALL=C
export LC_ALL

here=$(cd "$(dirname "$0")" && pwd -P)
repo=$(cd "$here/../../../.." && pwd -P)
src=$repo/usr/src/uts/intel/io/i915n/illumos/i915_illumos.c
cc=${CC:-cc}

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

[ -r "$src" ] || fail "cannot read $src"

tmp=$(mktemp -d "${TMPDIR:-/tmp}/dirtyfb-ranges-XXXXXX")
trap 'st=$?; rm -rf "$tmp"; exit $st' EXIT
trap 'exit 1' HUP INT TERM

# The span starts at the "#define<TAB>I915N_DIRTY_MAX_CLIPS" line and ends
# with the first line "}" after the line that starts with
# "i915n_dirty_ranges(".
st=0
awk '
{ line[NR] = $0 }
END {
	s = 0
	f = 0
	e = 0
	for (i = 1; i <= NR && !s; i++)
		if (index(line[i], "#define\tI915N_DIRTY_MAX_CLIPS\t") == 1)
			s = i
	if (!s)
		exit 2
	for (i = s + 1; i <= NR && !f; i++)
		if (index(line[i], "i915n_dirty_ranges(") == 1)
			f = i
	if (!f)
		exit 3
	for (i = f + 1; i <= NR && !e; i++)
		if (line[i] == "}")
			e = i
	if (!e)
		exit 4
	for (i = s; i <= e; i++)
		print line[i]
}' "$src" > "$tmp/span.c" || st=$?
case $st in
0)	;;
2)	fail "start marker '#define I915N_DIRTY_MAX_CLIPS' not found in $src" ;;
3)	fail "function 'i915n_dirty_ranges(' not found in $src" ;;
4)	fail "no closing '}' after 'i915n_dirty_ranges(' in $src" ;;
*)	fail "extracting i915n_dirty_ranges from $src (awk status $st)" ;;
esac
[ -s "$tmp/span.c" ] || fail "extracted i915n_dirty_ranges span is empty"

cat > "$tmp/pre.c" <<'EOF'
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint64_t u64;
struct drm_clip_rect { unsigned short x1, y1, x2, y2; };
EOF

cat > "$tmp/post.c" <<'EOF'

#define	W	1920u
#define	H	1080u
#define	P	(W * 4u)
#define	FB	((u64)P * H)

static struct i915n_dirty_range r[I915N_DIRTY_MAX_CLIPS];

static unsigned int
ranges(u64 off, u64 pitch, u64 w, u64 h, u64 size,
    const struct drm_clip_rect *c, unsigned int n)
{
	unsigned int got, i;

	for (i = 0; i < I915N_DIRTY_MAX_CLIPS; i++)
		r[i].start = r[i].end = 0xdeadbeefu;
	got = i915n_dirty_ranges(off, pitch, w, h, size, c, n, r);
	assert(got <= I915N_DIRTY_MAX_CLIPS);
	for (i = 0; i < got; i++) {
		/* Never empty, never outside the framebuffer or the object. */
		assert(r[i].start < r[i].end);
		assert(r[i].start >= off);
		assert(r[i].end <= off + pitch * h && r[i].end <= size);
	}
	return (got);
}

static void
is(unsigned int i, u64 start, u64 end)
{
	if (r[i].start != start || r[i].end != end) {
		fprintf(stderr, "range %u: [%llu, %llu), want [%llu, %llu)\n",
		    i, (unsigned long long)r[i].start,
		    (unsigned long long)r[i].end, (unsigned long long)start,
		    (unsigned long long)end);
		assert(0);
	}
}

int
main(void)
{
	struct drm_clip_rect c[I915N_DIRTY_MAX_CLIPS + 1];
	unsigned int i;

	/* No clips: the framebuffer once. */
	assert(ranges(0, P, W, H, FB, NULL, 0) == 1);
	is(0, 0, FB);

	/* One clip: its rows, whole. */
	c[0] = (struct drm_clip_rect){ 10, 100, 200, 110 };
	assert(ranges(0, P, W, H, FB, c, 1) == 1);
	is(0, 100ull * P, 110ull * P);

	/* Rows below the framebuffer are clamped to it. */
	c[0] = (struct drm_clip_rect){ 0, 1070, 10, 2000 };
	assert(ranges(0, P, W, H, FB, c, 1) == 1);
	is(0, 1070ull * P, FB);

	/* Inverted, empty and out-of-bounds clips flush nothing. */
	c[0] = (struct drm_clip_rect){ 0, 50, 10, 40 };	/* y */
	c[1] = (struct drm_clip_rect){ 20, 50, 10, 60 };	/* x */
	c[2] = (struct drm_clip_rect){ 5, 70, 5, 80 };	/* x1 == x2 */
	c[3] = (struct drm_clip_rect){ W, 70, W + 10, 80 };	/* right */
	c[4] = (struct drm_clip_rect){ 0, H, 10, H + 5 };	/* below */
	c[5] = (struct drm_clip_rect){ 0, 65535, 10, 65535 };
	assert(ranges(0, P, W, H, FB, c, 6) == 0);

	/* ... and are dropped among valid ones, keeping the order. */
	c[6] = (struct drm_clip_rect){ 0, 0, 1, 1 };
	c[7] = (struct drm_clip_rect){ 0, 500, W, 520 };
	assert(ranges(0, P, W, H, FB, &c[4], 4) == 2);
	is(0, 0, P);
	is(1, 500ull * P, 520ull * P);

	/* Exactly the clip bound: one range each. */
	for (i = 0; i < I915N_DIRTY_MAX_CLIPS; i++)
		c[i] = (struct drm_clip_rect){ 0, i * 10, 8, i * 10 + 2 };
	assert(ranges(0, P, W, H, FB, c, I915N_DIRTY_MAX_CLIPS) ==
	    I915N_DIRTY_MAX_CLIPS);
	for (i = 0; i < I915N_DIRTY_MAX_CLIPS; i++)
		is(i, (u64)i * 10 * P, ((u64)i * 10 + 2) * P);

	/* One clip more than the bound: the framebuffer once. */
	c[I915N_DIRTY_MAX_CLIPS] = (struct drm_clip_rect){ 0, 900, 8, 901 };
	assert(ranges(0, P, W, H, FB, c, I915N_DIRTY_MAX_CLIPS + 1) == 1);
	is(0, 0, FB);

	/* Half the rows still go clip by clip; more is the whole. */
	c[0] = (struct drm_clip_rect){ 0, 0, W, 270 };
	c[1] = (struct drm_clip_rect){ 0, 540, W, 810 };
	assert(ranges(0, P, W, H, FB, c, 2) == 2);
	is(0, 0, 270ull * P);
	is(1, 540ull * P, 810ull * P);
	c[1].y2 = 811;
	assert(ranges(0, P, W, H, FB, c, 2) == 1);
	is(0, 0, FB);

	/* Overlapping clips summing to more than the height: the whole. */
	for (i = 0; i < 3; i++)
		c[i] = (struct drm_clip_rect){ 0, 0, W, H };
	assert(ranges(0, P, W, H, FB, c, 3) == 1);
	is(0, 0, FB);

	/*
	 * Plane offset and a pitch wider than the rows; the object ends in
	 * the last row: ranges stop at the object's end.
	 */
	{
		u64 off = 4096, pitch = 8192, h = 100;
		u64 size = off + pitch * 99 + 100;

		assert(ranges(off, pitch, 1000, h, size, NULL, 0) == 1);
		is(0, off, size);
		c[0] = (struct drm_clip_rect){ 0, 99, 1, 100 };
		assert(ranges(off, pitch, 1000, h, size, c, 1) == 1);
		is(0, off + 99 * pitch, size);
		c[0] = (struct drm_clip_rect){ 0, 3, 1, 5 };
		assert(ranges(off, pitch, 1000, h, size, c, 1) == 1);
		is(0, off + 3 * pitch, off + 5 * pitch);

		/* Rows past the end of a short object are dropped. */
		size = off + pitch * 50;
		c[0] = (struct drm_clip_rect){ 0, 60, 1, 70 };
		c[1] = (struct drm_clip_rect){ 0, 49, 1, 51 };
		assert(ranges(off, pitch, 1000, h, size, c, 2) == 1);
		is(0, off + 49 * pitch, size);

		/* A plane starting at or past the object's end: nothing. */
		assert(ranges(size, pitch, 1000, h, size, NULL, 0) == 0);
		assert(ranges(size + 1, pitch, 1000, h, size, c, 1) == 0);
	}

	/* No pitch: nothing. */
	assert(ranges(0, 0, W, H, FB, NULL, 0) == 0);

	/* Largest uapi values: 64-bit arithmetic, no wrap. */
	{
		u64 pitch = 0xffffffffu, h = 65535;
		u64 size = pitch * h + 0xfffffff0u;

		c[0] = (struct drm_clip_rect){ 0, 65534, 65535, 65535 };
		assert(ranges(0xfffffff0u, pitch, 65535, h, size, c, 1) == 1);
		is(0, 0xfffffff0u + 65534 * pitch, size);
	}

	puts("PASS: no clips, one clip, clamping, invalid clips, clip and "
	    "row bounds, offset/pitch at the object's end, 64-bit range");
	return (0);
}
EOF

cat "$tmp/pre.c" "$tmp/span.c" "$tmp/post.c" > "$tmp/test.c"

"$cc" -std=c99 -Wall -Wextra -Werror "$tmp/test.c" -o "$tmp/test" ||
    fail "compiling $tmp/test.c with $cc"
"$tmp/test" || fail "dirtyfb range test program exited with status $?"
