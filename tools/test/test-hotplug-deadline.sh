#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
#
# Exercise the actual sysevent deadline function, including jiffies wrap.
#
# hotplug_queue() is cut out of the real drm_kms_hotplug.c and compiled
# between a fake jiffies clock and four boundary cases, each followed by a
# storm of calls that must not move the deadline. Everything lives in one
# mktemp -d directory, removed at exit.

set -eu
LC_ALL=C
export LC_ALL

here=$(cd "$(dirname "$0")" && pwd -P)
repo=$(cd "$here/../.." && pwd -P)
src=$repo/carried/drm_kms/illumos/drm_kms_hotplug.c
cc=${CC:-cc}

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

[ -r "$src" ] || fail "cannot read $src"

tmp=$(mktemp -d "${TMPDIR:-/tmp}/hotplug-deadline-XXXXXX")
trap 'st=$?; rm -rf "$tmp"; exit $st' EXIT
trap 'exit 1' HUP INT TERM

# The span starts at "static void" followed by "hotplug_queue" and stops
# before the blank line that precedes "static int" followed by
# "hotplug_emit".
st=0
awk '
{ line[NR] = $0 }
END {
	s = 0
	e = 0
	for (i = 1; i < NR && !s; i++)
		if (line[i] == "static void" && index(line[i + 1], "hotplug_queue") == 1)
			s = i
	if (!s)
		exit 2
	for (i = s + 2; i < NR && !e; i++)
		if (line[i] == "static int" && index(line[i + 1], "hotplug_emit") == 1)
			e = i
	if (!e)
		exit 3
	if (line[e - 1] != "")
		exit 4
	for (i = s; i < e - 1; i++)
		print line[i]
}' "$src" > "$tmp/span.c" || st=$?
case $st in
0)	;;
2)	fail "start marker 'static void' + 'hotplug_queue' not found in $src" ;;
3)	fail "end marker 'static int' + 'hotplug_emit' not found in $src" ;;
4)	fail "no blank line before 'static int' + 'hotplug_emit' in $src" ;;
*)	fail "extracting hotplug_queue from $src (awk status $st)" ;;
esac
[ -s "$tmp/span.c" ] || fail "extracted hotplug_queue span is empty"

cat > "$tmp/pre.c" <<'EOF'

#include <assert.h>
#include <limits.h>
#include <stdio.h>
struct delayed_work { int unused; };
struct drm_hotplug { unsigned long next; struct delayed_work work; };
static unsigned long clock_value, step, scheduled;
static int pending, reads;
static void *system_wq;
static unsigned long ticks(void) { unsigned long n=clock_value; clock_value+=step; reads++; return n; }
#define jiffies ticks()
#define msecs_to_jiffies(n) ((unsigned long)(n))
#define time_before(a,b) ((long)((a)-(b)) < 0)
#define max(a,b) ((a)>(b)?(a):(b))
static int queue_delayed_work(void *q,struct delayed_work *w,unsigned long delay) { if(pending) return 0; pending=1; scheduled=delay; return 1; }
EOF

cat > "$tmp/post.c" <<'EOF'

static void check(unsigned long now,unsigned long next,unsigned long expected) {
 struct drm_hotplug hp={.next=next}; clock_value=now; step=2; reads=pending=0;
 hotplug_queue(&hp); assert(reads==1); assert(scheduled==expected);
 for(int i=0;i<100;i++) hotplug_queue(&hp);
 assert(scheduled==expected); /* a storm does not extend the deadline */
}
int main(void) {
 check(10,11,250); /* crossing the next tick cannot underflow */
 check(1000,2000,1000);
 check(2000,1000,250);
 check(ULONG_MAX-100,400,501); /* unsigned clock wrap */
 puts("PASS: deadline edge, rate floor, overdue, wrap, pending storm");
}
EOF

cat "$tmp/pre.c" "$tmp/span.c" "$tmp/post.c" > "$tmp/test.c"

"$cc" -std=c99 -Wall -Wextra -Wno-unused-parameter "$tmp/test.c" \
    -o "$tmp/test" || fail "compiling $tmp/test.c with $cc"
"$tmp/test" || fail "deadline test program exited with status $?"
