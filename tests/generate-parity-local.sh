#!/bin/sh
# Generate host-local 72 dpi PNGs with this tree's rsvg-convert and the
# P19 RSVG_PARITY font map. Does not touch rust 2.62.3 oracle PNGs.
set -eu

if [ "$#" -lt 4 ]; then
    echo "usage: $0 <rsvg-convert> <parity-svg-dir> <local-ref-dir> <list-file>" >&2
    exit 2
fi

CONVERT=$1
SRC=$2
DST=$3
LIST=$4

if [ ! -x "$CONVERT" ]; then
    echo "rsvg-convert not executable: $CONVERT" >&2
    exit 1
fi

n=0
while IFS= read -r line || [ -n "$line" ]; do
    case "$line" in
        ''|'#'*) continue ;;
    esac
    svg="$SRC/$line"
    if [ ! -f "$svg" ]; then
        echo "missing SVG: $svg" >&2
        exit 1
    fi
    rel=${line%.svg}
    out="$DST/${rel}-ref.png"
    mkdir -p "$(dirname "$out")"
    echo "local-ref $line"
    RSVG_PARITY=1 "$CONVERT" -d 72 -p 72 -f png -o "$out" "$svg"
    n=$((n + 1))
done < "$LIST"

echo "wrote $n local refs under $DST"
