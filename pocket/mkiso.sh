#!/usr/bin/env bash
#
# Pack an Ecstatica data directory into an ISO 9660 image for an
# openfpgaOS data slot. The app mounts it read-only at /game.
#
#   ./mkiso.sh ../data/e1  ecstatica.iso            # DOS database + W/ enhanced set
#   ./mkiso.sh ../data/e2  ecstatica2.iso           # E2, 320x200 assets only
#   ./mkiso.sh ../data/e2  ecstatica2.iso --hires   # + E2's 640x480 backgrounds
#   ./mkiso.sh ../data/e1  ecstatica.iso --dos-only # skip W/, smaller image
#
# Only the game's own assets go in — the source trees carry IDA databases,
# disassembly listings, installers and frame dumps that would add gigabytes.
#
# Two roots
# ---------
# E1's CD layout is a DOS install with a nested W/ holding the Win95 one.
# init_data_roots() probes W/CODE/ECSTATIC.FAN and, when it resolves, makes W
# a second search root; enhanced_graphics then decides which root wins for the
# swappable presentation directories. The root database stays authoritative,
# so a DOS root still boots at 320x200 and the graphics toggle switches the
# backgrounds to the 640x480 set at runtime.
#
# Only the swappable directories are ever read from W, so W/FILES (~40 MB of
# database that is never consulted cross-root) is deliberately left out.

set -euo pipefail

SRC=${1:-}
OUT=${2:-}
shift 2 || true

WITH_HIRES=0
WITH_W=1
for arg in "$@"; do
    case "$arg" in
        --hires)    WITH_HIRES=1 ;;
        --dos-only) WITH_W=0 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

if [ -z "$SRC" ] || [ -z "$OUT" ]; then
    echo "usage: mkiso.sh <data-dir> <out.iso> [--hires] [--dos-only]" >&2
    exit 2
fi
if [ ! -d "$SRC" ]; then
    echo "no such data directory: $SRC" >&2
    exit 1
fi

# Asset directories, in the spellings both games use.
DIRS=(CODE FILES ACTORS ACTIONS SCENES REP ARCHIVE SOUNDS VIEWS VISIB
      GRAPHICS LOWGRAPH MUSIC)
[ "$WITH_HIRES" = 1 ] && DIRS+=(HIRES)

# What the enhanced root supplies: the swappable presentation set, plus CODE
# because init_data_roots() probes W/CODE/ECSTATIC.FAN to find the root at all.
W_DIRS=(CODE HIRES VIEWS VISIB GRAPHICS LOWGRAPH MUSIC)

# Root-level files the engine opens by name.
FILES=(E_CONFIG D_CONFIG OFFSETS OFF2 PALLETTE.RAW TITLE_S.RAW TSCREEN.RAW
       SHADOW.DAT SHADEMAP.DAT ANTIALIA.DAT AASGLOGO.RAW PSYGLOGO.RAW
       ASLOGO.RAW LANGUAG.TXT)

STAGE=$(mktemp -d "${TMPDIR:-/tmp}/ecstatica-iso.XXXXXX")
trap 'rm -rf "$STAGE"' EXIT

# Copy one entry, accepting either case and the trailing-dot spelling that
# comes from extracting a DOS volume ("E_CONFIG." round-trips as that, and the
# engine asks for "e_config"). Normalises to upper case in the image.
copy_ci() {
    local from=$1 to=$2 name=$3 upper lower cand
    upper=$(printf '%s' "$name" | tr '[:lower:]' '[:upper:]')
    lower=$(printf '%s' "$name" | tr '[:upper:]' '[:lower:]')
    for cand in "$name" "$upper" "$lower" "$name." "$upper." "$lower."; do
        if [ -e "$from/$cand" ]; then
            cp -R "$from/$cand" "$to/$upper"
            return 0
        fi
    done
    return 1
}

stage_root() {
    local from=$1 to=$2 label=$3; shift 3
    local dirs=("$@")
    mkdir -p "$to"
    for d in "${dirs[@]}"; do
        if copy_ci "$from" "$to" "$d"; then echo "  $label dir  $d"; fi
    done
    for f in "${FILES[@]}"; do
        if copy_ci "$from" "$to" "$f"; then echo "  $label file $f"; fi
    done
}

echo "staging from $SRC"
stage_root "$SRC" "$STAGE" " " "${DIRS[@]}"

# The enhanced root, if this is a CD layout that carries one.
W_SRC=""
for cand in W w; do
    [ -d "$SRC/$cand" ] && W_SRC="$SRC/$cand" && break
done
if [ -n "$W_SRC" ] && [ "$WITH_W" = 1 ]; then
    if [ -d "$W_SRC/CODE" ] || [ -d "$W_SRC/code" ]; then
        echo "enhanced root: $W_SRC"
        stage_root "$W_SRC" "$STAGE/W" "W" "${W_DIRS[@]}"
    else
        echo "  ! $W_SRC has no CODE/ — not an enhanced root, skipping"
    fi
elif [ -n "$W_SRC" ]; then
    echo "  (skipping $W_SRC, --dos-only)"
fi

if [ ! -d "$STAGE/CODE" ] && [ ! -d "$STAGE/FILES" ]; then
    echo "neither CODE/ nor FILES/ found under $SRC — wrong directory?" >&2
    exit 1
fi

# Drop anything the port never reads but that ships alongside the assets.
find "$STAGE" \( -name '*.exe' -o -name '*.EXE' -o -name '*.dll' -o -name '*.DLL' \
               -o -name '*.ppm' -o -name '*.log' -o -name '.DS_Store' \) -delete

echo "staged $(du -sh "$STAGE" | cut -f1)"

# All game names are 8.3 and the tree is at most two levels deep, so plain
# ISO 9660 carries them intact. Rock Ridge / Joliet are emitted where the tool
# supports it and harmlessly ignored by drivers that don't read them.
if command -v xorriso >/dev/null 2>&1; then
    xorriso -as mkisofs -iso-level 3 -R -J -N -V ECSTATICA -o "$OUT" "$STAGE"
elif command -v mkisofs >/dev/null 2>&1; then
    mkisofs -iso-level 3 -R -J -N -V ECSTATICA -o "$OUT" "$STAGE"
elif command -v genisoimage >/dev/null 2>&1; then
    genisoimage -iso-level 3 -R -J -N -V ECSTATICA -o "$OUT" "$STAGE"
elif command -v hdiutil >/dev/null 2>&1; then
    hdiutil makehybrid -iso -joliet -default-volume-name ECSTATICA -o "$OUT" "$STAGE"
else
    echo "need one of: xorriso, mkisofs, genisoimage, hdiutil" >&2
    exit 1
fi

echo "wrote $OUT ($(du -h "$OUT" | cut -f1))"
