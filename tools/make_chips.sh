#!/bin/sh
# Inline code/common/engine_signals.h into the Wokwi custom chip sources,
# because a Wokwi custom chip must be a single self-contained C file.
# Usage: tools/make_chips.sh   (run from the repository root)
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=$ROOT/code/wokwi/chips/src
OUT=$ROOT/code/wokwi/chips
for f in "$SRC"/*.chip.c; do
    b=$(basename "$f")
    awk -v hdr="$ROOT/code/common/engine_signals.h" '
        /^#include "engine_signals.h"/ {
            print "/* ---- begin inlined engine_signals.h ---- */"
            while ((getline line < hdr) > 0) print line
            print "/* ---- end inlined engine_signals.h ---- */"
            next }
        { print }' "$f" > "$OUT/$b"
    cp "$SRC/${b%.c}.json" "$OUT/"
    echo "generated $OUT/$b"
done
