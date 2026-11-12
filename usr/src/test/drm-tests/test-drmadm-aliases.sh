#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
#
# Host regression tests for drmadm selection; never touches the running OS.
#
# Run the actual shell script with fake system commands. The temporary copy
# redirects its fixed PATH, live-root paths and boot marker into the sandbox;
# production has no environment overrides for these paths.
# Real DDI attachment and boot-archive integration need a disposable illumos BE.
# Everything lives in one mktemp -d directory, removed at exit.

set -eu
LC_ALL=C
export LC_ALL

here=$(cd "$(dirname "$0")" && pwd -P)
repo=$(cd "$here/../../../.." && pwd -P)
drmadm_src=$repo/usr/src/cmd/drmadm/drmadm
[ -r "$drmadm_src" ] || { echo "FAIL: cannot read $drmadm_src" >&2; exit 1; }

tmp=$(mktemp -d "${TMPDIR:-/tmp}/drmadm-alias-test-XXXXXX")
trap 'st=$?; rm -rf "$tmp"; exit $st' EXIT
trap 'exit 1' HUP INT TERM

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

bindir=$tmp/bin
marker_dir=$tmp/run
marker=$marker_dir/drmadm.pending
mkdir "$bindir" "$marker_dir"
TEST_MARKER_DIR=$marker_dir
export TEST_MARKER_DIR

# One mock for the system commands, selected by its name. A failed
# expectation prints "mock assertion failed" and exits 1.
cat > "$bindir/pkg" <<'EOF'
#!/bin/sh
LC_ALL=C
export LC_ALL
name=${0##*/}
set -f
root=$TEST_ROOT
bad() {
	echo "mock $name: assertion failed: $*" >&2
	exit 1
}
# split: the words of $1, shell/shlex style (double quotes group), one
# per line.
split() {
	printf '%s\n' "$1" | awk '{
		w = ""; q = 0; have = 0
		for (i = 1; i <= length($0); i++) {
			c = substr($0, i, 1)
			if (c == "\"") { q = !q; have = 1 }
			else if (!q && (c == " " || c == "\t")) {
				if (have) print w
				w = ""; have = 0
			} else { w = w c; have = 1 }
		}
		if (have) print w
	}'
}
case $name in
pkg)
	if [ "${1-}" = -R ]; then
		[ "${2-}" = "$root" ] || bad "pkg -R ${2-} is not $root"
		shift 2
	fi
	[ $# -ge 1 ] || bad "pkg without a subcommand"
	if [ "$1" = contents ]; then
		[ -e "$root/legacy-manifest" ] && cat "$root/legacy-manifest" && echo
		exit 0
	fi
	for last; do :; done
	[ "$last" = driver/graphics/virtio-gpu ] && exit 1
	exit 0 ;;
bootadm)
	if [ "${TEST_LIVE:-no}" = yes ]; then
		[ $# -eq 1 ] && [ "$1" = update-archive ] || bad "bootadm $*"
	else
		[ $# -eq 3 ] && [ "$1" = update-archive ] && [ "$2" = -R ] &&
		    [ "$3" = "$root" ] || bad "bootadm $*"
	fi
	if [ -e "$root/fail-bootadm" ]; then
		mv "$root/fail-bootadm" "$root/bootadm-failed"
		exit 1
	fi
	exit 0 ;;
modinfo)
	echo ' Id Loadaddr Size Info Rev Module Name'
	exit 0 ;;
df)
	[ $# -eq 2 ] && [ "$1" = -n ] && [ "$2" = "$TEST_MARKER_DIR" ] || bad "df $*"
	[ ! -e "$root/fail-df" ] || exit 1
	echo "$2 : $(cat "$root/fs-type")"
	exit 0 ;;
date)
	[ $# -eq 1 ] && [ "$1" = +%s ] || bad "date $*"
	cat "$root/fake-date"
	exit 0 ;;
kstat)
	[ "$*" = '-p unix:0:system_misc:boot_time' ] || bad "kstat $*"
	echo 'unix:0:system_misc:boot_time 1'
	exit 0 ;;
update_drv)
	;;
*)
	bad "unknown mock name" ;;
esac
if [ "${TEST_LIVE:-no}" = yes ]; then
	[ "${1-}" = -n ] || bad "update_drv $*: not -n"
	shift
else
	[ $# -ge 3 ] && [ "$1" = -n ] && [ "$2" = -b ] && [ "$3" = "$root" ] ||
	    bad "update_drv $*: not -n -b $root"
	shift 3
fi
[ $# -eq 4 ] || bad "update_drv: $# arguments, expected 4"
operation=$1 flag=$2 values=$3 driver=$4
[ "$flag" = -i ] || bad "update_drv flag $flag"
case $operation in -a|-d) ;; *) bad "update_drv operation $operation" ;; esac
case $driver in i915|i915n) ;; *) bad "update_drv driver $driver" ;; esac
if [ "$operation" = -a ] && [ "$driver" = i915n ] && [ -e "$root/fail-add" ]; then
	exit 1
fi
if [ "$operation" = -a ] && [ "$driver" = i915 ] && [ -e "$root/fail-restore" ]; then
	exit 1
fi
p=$root/etc/driver_aliases
# rows: "DRIVER<tab>ALIAS" for every non-empty line of the file.
rows=$(while IFS= read -r line || [ -n "$line" ]; do
	[ -n "$line" ] || continue
	split "$line" | awk 'NR > 1 { printf "\t" } { printf "%s", $0 } END { print "" }'
done < "$p")
T=$(printf '\t')
if [ "$operation" = -d ]; then
	# Native update_drv removal takes one unquoted alias, not a list.
	case $values in *'"'*|*' '*) bad "update_drv -d takes one unquoted alias: $values" ;; esac
	rows=$(printf '%s\n' "$rows" | awk -F "$T" -v d="$driver" -v v="$values" \
	    'NF && !(NF == 2 && $1 == d && $2 == v)')
else
	for value in $(split "$values"); do
		printf '%s\n' "$rows" | awk -F "$T" -v d="$driver" -v v="$value" \
		    '$2 == v && $1 != d { f = 1 } END { exit f }' ||
		    bad "update_drv -a $value: bound to another driver"
		printf '%s\n' "$rows" | awk -F "$T" -v d="$driver" -v v="$value" \
		    'NF == 2 && $1 == d && $2 == v { f = 1 } END { exit !f }' ||
		    rows=$(printf '%s\n%s%s%s\n' "$rows" "$driver" "$T" "$value")
	done
fi
printf '%s\n' "$rows" | awk -F "$T" 'NF { printf "%s \"%s\"\n", $1, $2 }' > "$p"
EOF
chmod 755 "$bindir/pkg"
for name in update_drv bootadm modinfo df date kstat; do
	cp "$bindir/pkg" "$bindir/$name"
done

# The copy of drmadm with mocks and sandbox paths. Check every replacement
# count so changes in production cannot silently escape the sandbox.
script=$tmp/drmadm
n=$(awk '{ s = $0; while ((i = index(s, "PATH=/usr/bin:/usr/sbin:/sbin"))) { n++; s = substr(s, i + 1) } }
    END { print n + 0 }' "$drmadm_src")
[ "$n" -eq 1 ] || fail "drmadm has $n fixed PATH lines, expected 1"
NEWPATH="PATH='$bindir':/usr/bin:/bin:/usr/sbin:/sbin" awk '
    $0 == "BOOT_MARKER=/var/run/drmadm.pending" {
	$0 = "BOOT_MARKER=" ENVIRON["TEST_MARKER_DIR"] "/drmadm.pending"; marker++
    }
    $0 == "\t\techo \"/$1\"" {
	$0 = "\t\techo \"$TEST_ROOT/$1\""; root++
    }
    $0 == "\tfor l in /dev/dri/*; do" {
	$0 = "\tfor l in \"$TEST_ROOT\"/dev/dri/*; do"; dri++
    }
    /cands=\$\(awk/ {
	sub(/\/etc\/path_to_inst/, "\"$TEST_ROOT/etc/path_to_inst\""); inst++
    }
    /awk -v mperm=\/etc\/minor_perm/ {
	sub(/mperm=\/etc\/minor_perm/, "mperm=\"$TEST_ROOT/etc/minor_perm\""); perm++
    }
    {
	f = "PATH=/usr/bin:/usr/sbin:/sbin"
	if ((i = index($0, f)))
		$0 = substr($0, 1, i - 1) ENVIRON["NEWPATH"] substr($0, i + length(f))
	print
    }
    END { if (marker != 1 || root != 1 || dri != 1 || inst != 1 || perm != 1) exit 1 }
' "$drmadm_src" > "$script" || fail "sandbox path replacements changed"

fixture() {
	root=$tmp/$1
	mkdir -p "$root/etc/system.d" "$root/usr/share/drmadm"
	printf 'i915 200\ni915n 201\n' > "$root/etc/name_to_major"
	printf 'i915 "pci8086,166"\n' > "$root/etc/driver_aliases"
	printf 'driver name=i915 alias=pci8086,166\n' > "$root/legacy-manifest"
	printf 'pci8086,166\npci8086,1916\npci8086,5916\n' > "$root/usr/share/drmadm/i915n.aliases"
	cp "$repo/usr/src/data/drm/20-modesetting.conf" "$root/usr/share/drmadm/20-modesetting.conf"
	: > "$root/etc/minor_perm"
	printf 'tmpfs\n' > "$root/fs-type"
	printf '2000000000\n' > "$root/fake-date"
}

# run ROOT SELECTION [fail]: drmadm -R ROOT select SELECTION, which must
# succeed (or fail with "fail"); its stdout is left in $out.
run() {
	if TEST_ROOT=$1 /bin/sh "$script" -R "$1" select "$2" > "$tmp/stdout" 2> "$tmp/stderr"; then
		rc=0
	else
		rc=$?
	fi
	if [ "${3-}" = fail ]; then
		[ "$rc" -ne 0 ] || { cat "$tmp/stdout" "$tmp/stderr" >&2; fail "select $2 on $1 succeeded"; }
	else
		[ "$rc" -eq 0 ] || { cat "$tmp/stdout" "$tmp/stderr" >&2; fail "select $2 on $1 exited $rc"; }
	fi
	[ ! -e "$1/etc/system.d/.drmadm.lock" ] || fail "lock left behind in $1"
	out=$(cat "$tmp/stdout")
}

# bindings ROOT: "ALIAS DRIVER" lines of driver_aliases, the last binding
# of an alias winning, sorted.
bindings() {
	awk '{ v = $2; gsub(/"/, "", v); b[v] = $1 } END { for (v in b) print v, b[v] }' \
	    "$1/etc/driver_aliases" | sort
}

expect_bindings() {
	got=$(bindings "$1")
	want=$(printf '%s\n' "$2" | sort)
	[ "$got" = "$want" ] || fail "bindings of $1: got [$got], want [$want]"
}

bound_to() {
	bindings "$1" | awk -v a="$2" '$1 == a { print $2 }'
}

contains() {
	case $1 in *"$2"*) return 0 ;; esac
	return 1
}

# live ROOT EXPECT COMMAND...: run as the live root, entirely in the sandbox.
live() {
	live_root=$1 expect=$2
	shift 2
	if TEST_ROOT=$live_root TEST_LIVE=yes /bin/sh "$script" "$@" > "$tmp/stdout" 2> "$tmp/stderr"; then
		rc=0
	else
		rc=$?
	fi
	[ "$rc" -eq "$expect" ] || {
		cat "$tmp/stdout" "$tmp/stderr" >&2
		fail "live $* exited $rc, expected $expect"
	}
	[ ! -e "$live_root/etc/system.d/.drmadm.lock" ] || fail "live lock left behind"
	out=$(cat "$tmp/stdout")
}

expect_installed() {
	cmp -s "$root/usr/share/drmadm/20-modesetting.conf" "$root/etc/X11/xorg.conf.d/20-drmadm.conf" ||
	    fail "Xorg configuration not installed"
}

fixture round-trip
run "$root" new
expect_bindings "$root" "pci8086,166 i915n
pci8086,1916 i915n
pci8086,5916 i915n"
run "$root" new
contains "$out" "Nothing changed" || fail "second select new: no 'Nothing changed' in [$out]"
run "$root" legacy
expect_bindings "$root" "pci8086,166 i915"
echo "PASS new-only Skylake/Kaby Lake binding, idempotence and legacy round-trip"

fixture conflict
printf 'other "pci8086,1916"\n' >> "$root/etc/driver_aliases"
run "$root" new
contains "$out" "Left alone" || fail "conflict: no 'Left alone' in [$out]"
[ "$(bound_to "$root" pci8086,1916)" = other ] || fail "conflict: pci8086,1916 not left with other"
echo "PASS another driver's binding is preserved"

fixture rollback
cp "$root/etc/driver_aliases" "$tmp/before"
: > "$root/fail-add"
run "$root" new fail
cmp -s "$root/etc/driver_aliases" "$tmp/before" || fail "rollback: driver_aliases changed"
[ ! -e "$root/etc/system.d/driver:graphics:drmadm" ] || fail "rollback: selection fragment left"
echo "PASS failed add rolls back aliases and selection"

fixture removed-packages
run "$root" new
rm "$root/usr/share/drmadm/i915n.aliases" "$root/legacy-manifest"
run "$root" legacy
[ -z "$(bound_to "$root" pci8086,1916)" ] || fail "removed-packages: pci8086,1916 still bound"
[ -z "$(bound_to "$root" pci8086,5916)" ] || fail "removed-packages: pci8086,5916 still bound"
echo "PASS recorded new IDs never leak to legacy without package data"

fixture older-package
rm "$root/usr/share/drmadm/i915n.aliases"
run "$root" new
expect_bindings "$root" "pci8086,166 i915n"
echo "PASS older i915n packages without alias data remain usable"

fixture invalid-data
cp "$root/etc/driver_aliases" "$tmp/before"
printf 'pci8086,1916\nbad data\n' > "$root/usr/share/drmadm/i915n.aliases"
run "$root" new fail
cmp -s "$root/etc/driver_aliases" "$tmp/before" || fail "invalid-data: driver_aliases changed"
[ ! -e "$root/etc/system.d/driver:graphics:drmadm" ] || fail "invalid-data: selection fragment left"
echo "PASS invalid alias data fails before mutation"

fixture live-selection
live "$root" 0 select new
[ -f "$marker" ] || fail "live selection has no boot marker"
[ ! -e "$root/var/run" ] || fail "live marker used alternate-root path"
cp "$root/etc/system.d/driver:graphics:drmadm" "$tmp/fragment-before"
printf '3000000000\n' > "$root/fake-date"
live "$root" 0 select new
cmp -s "$root/etc/system.d/driver:graphics:drmadm" "$tmp/fragment-before" || fail "no-op changed timestamp"
[ -f "$marker" ] || fail "no-op lost pending marker"
live "$root" 0 perms
[ ! -e "$root/etc/X11/xorg.conf.d/20-drmadm.conf" ] || fail "pending selection installed Xorg configuration"
live "$root" 0 status
contains "$out" 'Selected stack: new, from the next boot (selected after this boot)' || fail "pending status"
echo "PASS live selection and no-op stay pending without installing Xorg configuration"

# Simulated reboot: tmpfs loses its marker, persistent selection stays.
rm "$marker"
live "$root" 0 perms
expect_installed
live "$root" 0 status
contains "$out" 'Selected stack: new, since the last boot' || fail "booted status"
live "$root" 0 select new
[ ! -e "$marker" ] || fail "booted no-op created marker"
cmp -s "$root/etc/system.d/driver:graphics:drmadm" "$tmp/fragment-before" || fail "booted no-op changed timestamp"
echo "PASS simulated reboot installs Xorg configuration and no-op remains in effect"

# Repairing a lost alias is a changed re-selection and must wait for boot.
awk '!/pci8086,1916/' "$root/etc/driver_aliases" > "$tmp/aliases"
mv "$tmp/aliases" "$root/etc/driver_aliases"
live "$root" 0 select new
[ -f "$marker" ] || fail "changed re-selection has no marker"
live "$root" 0 perms
[ ! -e "$root/etc/X11/xorg.conf.d/20-drmadm.conf" ] || fail "changed re-selection retained configuration"
echo "PASS changed re-selection becomes pending again"

live "$root" 0 select legacy
[ -f "$marker" ] || fail "legacy selection has no marker"
live "$root" 0 status
contains "$out" 'Selected stack: legacy, from the next boot' || fail "legacy pending status"
live "$root" 0 perms
[ ! -e "$root/etc/X11/xorg.conf.d/20-drmadm.conf" ] || fail "legacy retained owned Xorg configuration"
rm "$marker"
live "$root" 0 status
contains "$out" 'Selected stack: legacy, since the last boot' || fail "legacy booted status"
echo "PASS legacy selection, status and owned configuration removal"

fixture alternate-boot
run "$root" new
[ ! -e "$marker" ] && [ ! -e "$root/var/run" ] || fail "alternate-root selection wrote a marker"
grep -q '^\* drmadm-time: 2000000000$' "$root/etc/system.d/driver:graphics:drmadm" || fail "future timestamp fixture"
TEST_ROOT=$root /bin/sh "$script" -R "$root" perms > "$tmp/stdout" 2> "$tmp/stderr" || fail "alternate-root perms"
[ ! -e "$root/etc/X11/xorg.conf.d/20-drmadm.conf" ] || fail "alternate-root perms installed configuration"
live "$root" 0 perms
expect_installed
echo "PASS alternate-root selection boots with future timestamp and no marker on either root"

fixture old-format
printf '* drmadm-selected: new\n* drmadm-time: 2000000000\n' > "$root/etc/system.d/driver:graphics:drmadm"
live "$root" 0 perms
expect_installed
printf '* drmadm-selected: new\n* drmadm-time: invalid\n' > "$root/etc/system.d/driver:graphics:drmadm"
live "$root" 0 perms
expect_installed
printf '* drmadm-selected: new\n' > "$root/etc/system.d/driver:graphics:drmadm"
live "$root" 0 perms
expect_installed
echo "PASS old-format state without marker installs regardless of timestamp"

fixture live-rollback
: > "$root/fail-add"
live "$root" 1 select new
[ ! -e "$marker" ] || fail "rollback left new marker"
[ ! -e "$root/etc/system.d/driver:graphics:drmadm" ] || fail "rollback left fragment"
: > "$marker"
live "$root" 1 select new
[ -f "$marker" ] || fail "rollback removed previous marker"
rm "$marker"
echo "PASS complete alias rollback restores marker absence and presence"

fixture archive-rollback
live "$root" 0 select new
rm "$marker"
cp "$root/etc/system.d/driver:graphics:drmadm" "$tmp/fragment-before"
cp "$root/etc/driver_aliases" "$tmp/before"
: > "$root/fail-bootadm"
live "$root" 1 select legacy
[ ! -e "$marker" ] || fail "archive rollback left marker"
cmp -s "$root/etc/system.d/driver:graphics:drmadm" "$tmp/fragment-before" || fail "archive rollback changed fragment"
cmp -s "$root/etc/driver_aliases" "$tmp/before" || fail "archive rollback changed aliases"
live "$root" 0 perms
expect_installed
echo "PASS boot archive failure restores aliases, fragment and marker state"

fixture incomplete-rollback
: > "$root/fail-add"
: > "$root/fail-restore"
live "$root" 1 select new
[ -f "$marker" ] || fail "incomplete rollback lost marker"
grep -q '^\* drmadm-selected: new$' "$root/etc/system.d/driver:graphics:drmadm" || fail "incomplete rollback lost fragment"
live "$root" 0 perms
[ ! -e "$root/etc/X11/xorg.conf.d/20-drmadm.conf" ] || fail "incomplete rollback enabled configuration"
rm "$marker"
echo "PASS incomplete rollback retains new fragment and pending marker"

fixture nonvolatile
printf 'zfs\n' > "$root/fs-type"
live "$root" 1 select new
[ ! -e "$marker" ] && [ ! -e "$root/etc/system.d/driver:graphics:drmadm" ] || fail "non-tmpfs selection mutated state"
contains "$(cat "$tmp/stderr")" 'must be on tmpfs' || fail "non-tmpfs diagnostic"
: > "$root/fail-df"
live "$root" 1 select new
[ ! -e "$marker" ] && [ ! -e "$root/etc/system.d/driver:graphics:drmadm" ] || fail "failed df mutated state"
run "$root" new
[ ! -e "$marker" ] && [ ! -e "$root/var/run" ] || fail "alternate root used marker on non-tmpfs"
echo "PASS non-tmpfs or unknown filesystem rejects live changes but permits alternate-root selection"

fixture marker-failure
rmdir "$marker_dir"
live "$root" 1 select new
[ ! -e "$root/etc/system.d/driver:graphics:drmadm" ] || fail "marker creation failure left fragment"
expect_bindings "$root" 'pci8086,166 i915'
mkdir "$marker_dir"
echo "PASS marker creation failure leaves selection and aliases unchanged"
