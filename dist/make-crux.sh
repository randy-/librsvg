#!/bin/bash
# Write ../pkg/Pkgfile next to librsvg-c-VERSION.tar.xz.
# Does not run pkgmk. Does not install onto the live host.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
src=$(cd "$here/.." && pwd)
pkgdir=$(cd "$src/.." && pwd)/pkg
template="$here/crux/Pkgfile"

major=$(sed -n 's/^m4_define(\[rsvg_major_version\],\[\([0-9]*\)\])/\1/p' "$src/configure.ac")
minor=$(sed -n 's/^m4_define(\[rsvg_minor_version\],\[\([0-9]*\)\])/\1/p' "$src/configure.ac")
micro=$(sed -n 's/^m4_define(\[rsvg_micro_version\],\[\([0-9]*\)\])/\1/p' "$src/configure.ac")
ver="${major}.${minor}.${micro}"
tarball="${pkgdir}/librsvg-c-${ver}.tar.xz"
out="${pkgdir}/Pkgfile"

if [ ! -f "$template" ]; then
	echo "$0: missing $template" >&2
	exit 1
fi
if [ ! -f "$tarball" ]; then
	echo "$0: missing $tarball (run make pkg first)" >&2
	exit 1
fi

mkdir -p "$pkgdir"
sed "s/^version=.*/version=$ver/" "$template" > "$out"

echo "Wrote $out"
echo "Source tarball $tarball"
echo "This target does not run pkgmk and does not install onto the host."
ls -l "$out" "$tarball"
