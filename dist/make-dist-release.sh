#!/bin/bash
# Build librsvg-c-VERSION.tar.xz under ../pkg/ (parent of this tree).
# Never install onto the live host. No rust/ in the archive.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
src=$(cd "$here/.." && pwd)
pkgdir=$(cd "$src/.." && pwd)/pkg

major=$(sed -n 's/^m4_define(\[rsvg_major_version\],\[\([0-9]*\)\])/\1/p' "$src/configure.ac")
minor=$(sed -n 's/^m4_define(\[rsvg_minor_version\],\[\([0-9]*\)\])/\1/p' "$src/configure.ac")
micro=$(sed -n 's/^m4_define(\[rsvg_micro_version\],\[\([0-9]*\)\])/\1/p' "$src/configure.ac")
ver="${major}.${minor}.${micro}"
name="librsvg-c-${ver}"
out="${pkgdir}/${name}.tar.xz"

mkdir -p "$pkgdir"

stage=$(mktemp -d)
pack_ignore=
cleanup() {
	if [ -n "${stage:-}" ] && [ -d "$stage" ]; then
		chmod -R u+w "$stage" 2>/dev/null || true
		rm -rf "$stage"
	fi
	if [ -n "${pack_ignore:-}" ]; then
		rm -f "$pack_ignore"
	fi
}
trap cleanup EXIT
mkdir -p "$stage/$name"

# Public source tree only. Packaging lives in git dist/, not this archive.
# rsync does not honor .gitignore by itself (that is how debug/*.log leaked).
# Use .gitignore plus dist/rsync-excludes. .gitignore has /build/ for local
# leftovers, but the source tree's build/ (Makefile.in, win32) must ship,
# so that one pattern is dropped from the gitignore copy.
excludes="$here/rsync-excludes"
if [ ! -f "$excludes" ]; then
	echo "$0: missing $excludes" >&2
	exit 1
fi
pack_ignore=$(mktemp)
grep -vx '/build/' "$src/.gitignore" > "$pack_ignore"
rsync -a \
	--exclude-from="$pack_ignore" \
	--exclude-from="$excludes" \
	"$src"/ "$stage/$name"/
rm -f "$pack_ignore"

# Belt-and-suspenders: drop maintainer-only leftovers rsync might keep.
rm -rf "$stage/$name/debug" "$stage/$name/dist" "$stage/$name/.git" \
	"$stage/$name/rust" "$stage/$name/debian" "$stage/$name/stage" \
	"$stage/$name/pkg" "$stage/$name/debpkg" "$stage/$name/tmp" \
	"$stage/$name/.downloads"
find "$stage/$name" -type d -name autom4te.cache -prune -exec rm -rf {} +
find "$stage/$name" \( -name '*.log' -o -name '*.trs' \) -delete
rm -f "$stage/$name/.gitignore"
find "$stage/$name" -name '.gitignore' -delete
find "$stage/$name" -name '*.asc' -delete
rm -f "$stage/$name/AGENTS.md" "$stage/$name/AGENTS_history.md" \
	"$stage/$name/AGENTS_deprecated-abi.md" "$stage/$name/PLAN.md"

if [ -e "$stage/$name/debug" ] || [ -e "$stage/$name/rust" ] \
	|| [ -e "$stage/$name/.git" ] || [ -e "$stage/$name/dist" ] \
	|| [ -e "$stage/$name/.gitignore" ] \
	|| [ -e "$stage/$name/AGENTS.md" ] \
	|| [ -e "$stage/$name/AGENTS_history.md" ] \
	|| [ -e "$stage/$name/AGENTS_deprecated-abi.md" ]; then
	echo "$0: maintainer-only files leaked into the staging tree" >&2
	exit 1
fi
if find "$stage/$name/tests" \( -name '*.log' -o -name '*.trs' \) | grep -q .; then
	echo "$0: test logs leaked into the staging tree" >&2
	exit 1
fi

tar -C "$stage" -cJf "$out" "$name"

if tar tJf "$out" | grep -E '(^|/)debug(/|$)|(^|/)tests/[^/]*\.(log|trs)$|(^|/)\.git(/|$)|(^|/)rust(/|$)|(^|/)dist(/|$)|(^|/)pkg(/|$)|autom4te\.cache|(^|/)\.gitignore$|\.asc$|\.libs/|\.o$|(^|/)AGENTS(_history|_deprecated-abi)?\.md$' >/dev/null; then
	echo "$0: archive contains debug/, test logs, dist/, pkg/, rust/, .git, autom4te.cache, .libs, objects, or AGENTS maintainer files" >&2
	rm -f "$out"
	exit 1
fi

echo "Wrote $out"
ls -l "$out"
