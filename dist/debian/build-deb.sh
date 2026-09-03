#!/bin/bash
# Build librsvg-c binary packages into ../debpkg/ (parent of the source
# tree). Never install onto the live host.
set -euo pipefail

KEY=2F4939F1142E522362100BD39CA0BE9629EAB796

here=$(cd "$(dirname "$0")" && pwd)
src=$(cd "$here/../.." && pwd)
debroot=$(cd "$src/.." && pwd)
debpkg="$debroot/debpkg"

case "${1:-}" in
  -h|--help)
    echo "usage: $0"
    echo "Build librsvg-c with dpkg-buildpackage -b and write artifacts to:"
    echo "  $debpkg"
    echo "Does not dpkg -i or make install onto the host."
    exit 0
    ;;
  install|--install|-i)
    echo "$0: refusing to install onto the host" >&2
    exit 1
    ;;
esac

mkdir -p "$debpkg"

cd "$src"
ln -sfn dist/debian debian
if [ ! -f debian/control ] || [ ! -L debian ]; then
  echo "$0: debian/ must be a symlink to dist/debian" >&2
  exit 1
fi

collect_artifacts() {
  local dir f
  shopt -s nullglob
  for dir in "$src" "$debroot"; do
    [ "$dir" = "$debpkg" ] && continue
    for f in \
      "$dir"/librsvg-c*.deb \
      "$dir"/librsvg-c*.changes \
      "$dir"/librsvg-c*.buildinfo \
      "$dir"/librsvg-c*.dsc \
      "$dir"/librsvg-c*.tar.gz \
      "$dir"/librsvg-c*.tar.xz \
      "$dir"/librsvg-c*.tar.zst \
      "$dir"/librsvg-c*.diff.gz
    do
      [ -e "$f" ] || continue
      mv -f "$f" "$debpkg/"
    done
  done
  shopt -u nullglob
}

# Sweep leftovers from earlier builds first.
collect_artifacts

if [ -t 1 ] && [ -z "${GPG_TTY:-}" ]; then
  GPG_TTY=$(tty) || true
  export GPG_TTY
fi

# Do not pass -tc (clean after): that can distclean shipped croco headers.
# debian/rules already uses DESTDIR only; this script never dpkg -i.
set +e
dpkg-buildpackage -b -k"$KEY"
build_rc=$?
set -e

collect_artifacts

if [ "$build_rc" -ne 0 ]; then
  shopt -s nullglob
  new_debs=("$src"/librsvg-c*.deb "$debroot"/librsvg-c*.deb)
  shopt -u nullglob
  # Ignore leftovers already in debpkg/; retry if this run produced nothing.
  if [ ${#new_debs[@]} -eq 0 ]; then
    echo "$0: signed build failed; retrying unsigned (-us -uc)" >&2
    dpkg-buildpackage -b -us -uc
  fi
  collect_artifacts
  shopt -s nullglob
  changes=("$debpkg"/librsvg-c_*.changes)
  shopt -u nullglob
  if [ ${#changes[@]} -gt 0 ]; then
    echo "$0: signing ${changes[*]} with $KEY" >&2
    debsign -k"$KEY" "${changes[@]}" || echo "$0: debsign failed; unsigned artifacts remain in $debpkg" >&2
  fi
fi

collect_artifacts

# Fail if anything still sits next to the source tree or in it.
left=
shopt -s nullglob
for f in \
  "$src"/librsvg-c*.deb "$src"/librsvg-c*.changes "$src"/librsvg-c*.buildinfo \
  "$debroot"/librsvg-c*.deb "$debroot"/librsvg-c*.changes "$debroot"/librsvg-c*.buildinfo \
  "$debroot"/librsvg-c*.dsc "$debroot"/librsvg-c*.tar.*
do
  case "$f" in
    "$debpkg"/*) continue ;;
  esac
  [ -e "$f" ] || continue
  left="$left $f"
done
shopt -u nullglob

if [ -n "$left" ]; then
  echo "$0: artifacts still outside $debpkg:$left" >&2
  exit 1
fi

# Install docs next to the debs (durable copies live in deb/).
if [ -f "$src/deb/INSTALL.md" ]; then
	cp -f "$src/deb/INSTALL.md" "$debpkg/INSTALL.md"
fi
if [ -f "$src/deb/install.sh" ]; then
	install -m 0755 "$src/deb/install.sh" "$debpkg/install.sh"
fi

echo "Artifacts in $debpkg:"
ls -l "$debpkg"
