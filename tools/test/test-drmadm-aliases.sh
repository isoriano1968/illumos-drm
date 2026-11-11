#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
#
# Host regression tests for drmadm selection; never touches the running OS.
#
# Run the actual shell script in an alternate root with fake pkg/update_drv/
# bootadm commands. Only its fixed PATH is replaced in the temporary copy.
# Real DDI attachment and boot-archive integration need a disposable illumos BE.
# Everything lives in one mktemp -d directory, removed at exit.

set -eu
LC_ALL=C
export LC_ALL

here=$(cd "$(dirname "$0")" && pwd -P)
repo=$(cd "$here/../.." && pwd -P)
drmadm_src=$repo/tools/drmadm/drmadm
[ -r "$drmadm_src" ] || { echo "FAIL: cannot read $drmadm_src" >&2; exit 1; }

tmp=$(mktemp -d "${TMPDIR:-/tmp}/drmadm-alias-test-XXXXXX")
trap 'st=$?; rm -rf "$tmp"; exit $st' EXIT
trap 'exit 1' HUP INT TERM

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

bindir=$tmp/bin
mkdir "$bindir"

# One mock for all three commands, selected by its name. A failed
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
	[ $# -eq 3 ] && [ "$1" = update-archive ] && [ "$2" = -R ] &&
	    [ "$3" = "$root" ] || bad "bootadm $*"
	exit 0 ;;
update_drv)
	;;
*)
	bad "unknown mock name" ;;
esac
[ $# -ge 3 ] && [ "$1" = -n ] && [ "$2" = -b ] && [ "$3" = "$root" ] ||
	bad "update_drv $*: not -n -b $root"
shift 3
[ $# -eq 4 ] || bad "update_drv: $# arguments, expected 4"
operation=$1 flag=$2 values=$3 driver=$4
[ "$flag" = -i ] || bad "update_drv flag $flag"
case $operation in -a|-d) ;; *) bad "update_drv operation $operation" ;; esac
case $driver in i915|i915n) ;; *) bad "update_drv driver $driver" ;; esac
if [ "$operation" = -a ] && [ "$driver" = i915n ] && [ -e "$root/fail-add" ]; then
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
cp "$bindir/pkg" "$bindir/update_drv"
cp "$bindir/pkg" "$bindir/bootadm"

# The copy of drmadm with the mocks first on its fixed PATH.
script=$tmp/drmadm
n=$(awk '{ s = $0; while ((i = index(s, "PATH=/usr/bin:/usr/sbin:/sbin"))) { n++; s = substr(s, i + 1) } }
    END { print n + 0 }' "$drmadm_src")
[ "$n" -eq 1 ] || fail "drmadm has $n fixed PATH lines, expected 1"
NEWPATH="PATH='$bindir':/usr/bin:/bin:/usr/sbin:/sbin" awk '{
	f = "PATH=/usr/bin:/usr/sbin:/sbin"
	if ((i = index($0, f)))
		$0 = substr($0, 1, i - 1) ENVIRON["NEWPATH"] substr($0, i + length(f))
	print
}' "$drmadm_src" > "$script"

fixture() {
	root=$tmp/$1
	mkdir -p "$root/etc/system.d" "$root/usr/share/drmadm"
	printf 'i915 200\ni915n 201\n' > "$root/etc/name_to_major"
	printf 'i915 "pci8086,166"\n' > "$root/etc/driver_aliases"
	printf 'driver name=i915 alias=pci8086,166\n' > "$root/legacy-manifest"
	printf 'pci8086,166\npci8086,1916\npci8086,5916\n' > "$root/usr/share/drmadm/i915n.aliases"
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
