#!/bin/bash
#
# SPDX-License-Identifier: MIT
# Copyright 2026 The illumos-drm authors
#
# Prepares a Linux tree for the amdgpu build, the way
# drivers/i915/fetch-and-patch.sh does for i915n: checks the pinned commit
# (PIN), applies misc/drm_kms' patches and then amdgpu's
# (carried/drm_kms/patches/*.patch, drivers/amdgpu/patches/*.patch),
# writes the MIT replacements of GPL-2.0 headers amdgpu includes
# (REPLACED), and copies the MIT headers of mit-headers.txt into
# <tree>/.illumos/mit-include. The licence check refuses the tree if a
# TU of tus.txt or a header of mit-headers.txt is not MIT (or offers MIT);
# build.sh checks every Linux file the compiler read as well.
#
# The tree is amdgpu's own (not i915n's: the two series differ), e.g. a
# git worktree of the i915n tree at the same commit.
#
#   drivers/amdgpu/fetch-and-patch.sh <directory>
set -euo pipefail
PATH=/usr/gnu/bin:$PATH
export LC_ALL=C

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
# shellcheck disable=SC1091
. "$HERE/PIN"
dest=${1:?usage: fetch-and-patch.sh <directory>}
AMD=drivers/gpu/drm/amd/amdgpu

if ! git -C "$dest" rev-parse --git-dir >/dev/null 2>&1; then
	mkdir -p "$dest"
	git -C "$dest" init -q
	git -C "$dest" remote add origin "$LINUX_REPO"
	git -C "$dest" fetch -q --depth 1 origin "$LINUX_REV"
	git -C "$dest" checkout -q FETCH_HEAD
fi
[ "$(git -C "$dest" rev-parse HEAD)" = "$LINUX_REV" ] ||
    { echo "fetch-and-patch: $dest is not at the pinned $LINUX_REV" >&2; exit 1; }

# tree path, then the MIT replacement relative to the repository
REPLACED="$AMD/amdgpu_vkms.h drivers/amdgpu/illumos/amdgpu_vkms.h"
REPLACED="$REPLACED drivers/gpu/drm/display/drm_dp_mst_topology_internal.h"
REPLACED="$REPLACED carried/drm_kms/illumos/drm_dp_mst_topology_internal.h"
replaced_paths() { echo $REPLACED | awk '{ for (i = 1; i <= NF; i += 2) print $i }'; }

series=$( (cd "$REPO" && { ls carried/drm_kms/patches/*.patch 2>/dev/null | sort
    ls drivers/amdgpu/patches/*.patch 2>/dev/null | sort; } || true) )
sum=$(cd "$REPO" && cat /dev/null $series | sha256sum | cut -d' ' -f1)
stamp=$dest/.illumos/applied
if [ -f "$stamp" ] && [ "$(sed -n 1p "$stamp")" = "$sum" ]; then
	echo "fetch-and-patch: series already applied ($sum)"
else
	# shellcheck disable=SC2046
	git -C "$dest" checkout -- $(replaced_paths)
	if [ -f "$stamp" ] && [ -d "$dest/.illumos/series" ]; then
		for p in $(ls "$dest/.illumos/series" | sort -r); do
			echo "fetch-and-patch: reverting the earlier $p"
			git -C "$dest" apply -R --whitespace=nowarn "$dest/.illumos/series/$p"
		done
		rm -rf "$dest/.illumos/series" "$stamp"
	fi
	if [ -n "$(git -C "$dest" status --porcelain --untracked-files=no)" ]; then
		echo "fetch-and-patch: $dest has changes that are not this series" >&2
		exit 1
	fi
	mkdir -p "$dest/.illumos/series"
	i=0
	for p in $series; do
		i=$((i + 1))
		echo "fetch-and-patch: applying $p"
		git -C "$dest" apply --whitespace=nowarn "$REPO/$p"
		cp "$REPO/$p" "$dest/.illumos/series/$(printf '%03d' $i)-$(basename "$p")"
	done
	{ echo "$sum"; echo "$series"; } > "$stamp"
fi

set -- $REPLACED
while [ $# -ge 2 ]; do cp "$REPO/$2" "$dest/$1"; shift 2; done

# Licences: the TUs (amdgpu-relative, "/" tree-relative) and the headers.
{
	grep -v '^#' "$HERE/tus.txt" | grep . | grep -v '^illumos/' |
	    sed -e "s|^/||;t" -e "s|^|$AMD/|"
	grep -v '^#' "$HERE/mit-headers.txt" | grep .
} | sed 's|/[^/]*/\.\./|/|' > "$dest/.illumos/licence-check.txt"
python3 "$REPO/drivers/i915/check-licences.py" "$dest" "$dest/.illumos/licence-check.txt"

mit=$dest/.illumos/mit-include
rm -rf "$mit"
mkdir -p "$mit"
grep -v '^#' "$HERE/mit-headers.txt" | grep . | while read -r f; do
	d=$mit/${f#include/}
	mkdir -p "$(dirname "$d")"
	cp "$dest/$f" "$d"
done
echo "fetch-and-patch: $dest ready (Linux $LINUX_TAG, $(echo "$series" | grep -c . || true) patches)"
