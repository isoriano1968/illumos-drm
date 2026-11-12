#!/bin/sh
# SPDX-License-Identifier: MIT
# Copyright 2026 The illumos-drm authors
# Read-only comparison; no checkout, patching, temporary files or Git writes.
# Update upstream_rev with SOURCES when importing a new upstream revision.
set -u
LC_ALL=C
export LC_ALL
upstream_rev=8d3ae59288f1e7d58d76558a6ee96d533bc5019f
here=$(CDPATH= cd "$(dirname "$0")" && pwd) || exit 1
repo=$(CDPATH= cd "$here/../../../.." && pwd) || exit 1
failed=0
pristine=
inputs=
nl='
'
usage() {
    echo "usage: $0 [-p pristine-checkout] [-i linux-inputs.txt]... fetched-tree" >&2
    echo 'RESULT: FAIL'
    exit 2
}
while [ "$#" -gt 0 ]; do
    case "$1" in
    -p) [ "$#" -ge 2 ] || usage; pristine=$2; shift 2 ;;
    -i) [ "$#" -ge 2 ] || usage; inputs=$inputs$nl$2; shift 2 ;;
    --) shift; break ;;
    -*) usage ;;
    *) break ;;
    esac
done
[ "$#" -eq 1 ] || usage
fetched=$1
fail() { echo "FAIL: $*"; failed=1; }
# Lexical normalization only: never resolve symlinks. Normalize before
# mapping the fetched tree's copied-header namespace, then normalize again.
normalize() {
    awk '
    function norm(s,    n,a,i,k,r) {
        if (s ~ /^\//) { print "absolute input: " s > "/dev/stderr"; exit 1 }
        n=split(s,a,"/"); k=0
        for(i=1;i<=n;i++) {
            if(a[i]=="" || a[i]==".") continue
            if(a[i]=="..") {
                if(!k) { print "escaping input: " s > "/dev/stderr"; exit 1 }
                k--
            } else b[++k]=a[i]
        }
        r=""; for(i=1;i<=k;i++) r=r (i==1?"":"/") b[i]
        return r
    }
    NF { s=norm($0); sub(/^\.illumos\/mit-include\//,"include/",s); print norm(s) }'
}
sha256() {
    if command -v digest >/dev/null 2>&1; then
        digest -a sha256
    elif command -v sha256sum >/dev/null 2>&1; then
        sha256sum | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 | awk '{print $1}'
    else
        echo 'no SHA-256 tool found' >&2
        return 1
    fi
}
if [ ! -f "$here/SOURCES" ] || [ ! -d "$fetched" ]; then
    echo 'FAIL: SOURCES or fetched tree missing'
    echo 'RESULT: FAIL'
    exit 1
fi
if [ -n "$pristine" ]; then
    if ! git -C "$pristine" cat-file -e "$upstream_rev^{commit}"; then
        echo "FAIL: pristine checkout lacks $upstream_rev"
        echo 'RESULT: FAIL'
        exit 1
    fi
fi
sources=0
pristine_ok=0
tab=$(printf '\t')
while IFS="$tab" read -r rel expected licence; do
    [ -n "$rel" ] || continue
    sources=$((sources + 1))
    canonical=$(printf '%s\n' "$rel" | normalize) || {
        fail "invalid SOURCES path: $rel"; continue;
    }
    if [ "$canonical" != "$rel" ] || [ -z "$licence" ] ||
        [ "${#expected}" -ne 64 ]; then
        fail "invalid SOURCES record: $rel"
        continue
    fi
    case "$expected" in *[!0-9a-f]*) fail "invalid SHA-256: $rel"; continue ;; esac
    [ -f "$here/$rel" ] || fail "missing vendored file: $rel"
    if [ -n "$pristine" ]; then
        if ! git -C "$pristine" cat-file -e "$upstream_rev:$rel"; then
            fail "missing pristine file: $rel"
            continue
        fi
        got=$(git -C "$pristine" show "$upstream_rev:$rel" | sha256) || {
            fail "cannot hash pristine file: $rel"; continue;
        }
        if [ "$got" = "$expected" ]; then
            pristine_ok=$((pristine_ok + 1))
        else
            fail "pristine digest differs: $rel (expected $expected, got $got)"
        fi
    fi
done < "$here/SOURCES"
[ "$sources" -gt 0 ] || fail 'empty SOURCES'
# These four repository metadata/tool files are not upstream source inputs.
files=$(cd "$here" && find . -type f -print) || {
    echo 'FAIL: cannot enumerate vendored tree'; echo 'RESULT: FAIL'; exit 1;
}
files=$(printf '%s\n' "$files" | sed 's|^\./||' | awk '
    $0 != ".gitattributes" && $0 != "SOURCES" &&
    $0 != "README" && $0 != "verify-vendored.sh"' | sort)
links=$(cd "$here" && find . -type l -print) || fail 'cannot enumerate symlinks'
[ -z "$links" ] || fail "unexpected symlinks: $links"
compared=0
headers=0
while IFS= read -r rel; do
    [ -n "$rel" ] || continue
    compared=$((compared + 1))
    if ! cmp -s "$here/$rel" "$fetched/$rel"; then
        fail "fetched file differs or is missing: $rel"
    fi
    case "$rel" in
    include/*)
        headers=$((headers + 1))
        if ! cmp -s "$here/$rel" "$fetched/.illumos/mit-include/${rel#include/}"; then
            fail "mit-include copy differs or is missing: $rel"
        fi ;;
    esac
done <<END_FILES
$files
END_FILES
lists=0
input_count=0
while IFS= read -r listing; do
    [ -n "$listing" ] || continue
    lists=$((lists + 1))
    if [ ! -r "$listing" ]; then fail "cannot read input list: $listing"; continue; fi
    normalized=$(normalize < "$listing") || {
        fail "invalid input list: $listing"; continue;
    }
    [ -n "$normalized" ] || fail "empty input list: $listing"
    while IFS= read -r rel; do
        [ -n "$rel" ] || continue
        input_count=$((input_count + 1))
        [ -f "$here/$rel" ] || fail "build input missing: $rel (from $listing)"
    done <<END_INPUTS
$normalized
END_INPUTS
done <<END_LISTS
$inputs
END_LISTS
# Use the existing authoritative gate, rather than duplicating its Python
# rules in shell. /dev/fd/0 supplies the inventory without a temporary file.
# It checks the first 6000 characters and first 60 lines for SPDX MIT
# alternatives (rejecting AND/WITH), or an MIT/HPND permission notice.
if [ ! -f "$repo/drivers/i915/check-licences.py" ]; then
    fail 'existing check-licences.py not found'
elif ! printf '%s\n' "$files" |
    python3 "$repo/drivers/i915/check-licences.py" "$here" /dev/fd/0; then
    fail 'vendored licence check'
fi
echo "SOURCES: $sources entries checked; pristine digests verified: $pristine_ok"
echo "Comparison: $compared files; $headers mit-include copies"
echo "Build inputs: $lists lists; $input_count paths checked"
if [ "$failed" -eq 0 ]; then
    echo 'RESULT: PASS'
    exit 0
fi
echo 'RESULT: FAIL'
exit 1
