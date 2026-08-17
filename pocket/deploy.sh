#!/usr/bin/env bash
#
# Sync pocket/build/ onto an Analogue Pocket SD card.
#
#   ./deploy.sh build            auto-detect the card
#   ./deploy.sh build /Volumes/AP  explicit mount point
#
# Only this core's own subdirectories are synced with --delete. The parent
# Cores/ and Assets/ directories are never touched as a whole — deleting
# there would wipe every other core on the card.

set -euo pipefail

BUILD=${1:-build}
SDCARD=${2:-${SDCARD:-}}

GREEN='\033[92m'; RED='\033[91m'; RESET='\033[0m'
ok()   { printf "  ${GREEN}+${RESET} %s\n" "$1"; }
fail() { printf "  ${RED}x${RESET} %s\n" "$1"; exit 1; }

[ -d "$BUILD/Cores" ] || fail "$BUILD/ not assembled — run 'make release' first."

# A Pocket card is any mount with both Cores/ and Assets/ at its root.
if [ -z "$SDCARD" ]; then
    case "$(uname -s)" in
        Darwin) candidates=(/Volumes/*) ;;
        Linux)
            if grep -qi microsoft /proc/version 2>/dev/null; then
                candidates=(/mnt/[a-z]/*)
            else
                candidates=(/run/media/"$USER"/* /media/"$USER"/* /mnt/*)
            fi ;;
        *) candidates=(/[a-z]/*) ;;
    esac
    for m in "${candidates[@]}"; do
        if [ -d "$m/Cores" ] && [ -d "$m/Assets" ]; then SDCARD=$m; break; fi
    done
fi

[ -n "$SDCARD" ] || fail "no Pocket SD card found — pass the mount point explicitly."
[ -d "$SDCARD/Cores" ] || fail "$SDCARD does not look like a Pocket card (no Cores/)."

echo "Deploying $BUILD/ -> $SDCARD"

# --checksum, not timestamps: FAT32 mtimes are too coarse to trust, and a
# stale ELF that looks current is a confusing failure to debug.
RSYNC=(rsync --recursive --checksum --times --delete --human-readable --out-format="    %n")

for group in Cores Assets Platforms; do
    [ -d "$BUILD/$group" ] || continue
    for src in "$BUILD/$group"/*/; do
        [ -d "$src" ] || continue
        name=$(basename "$src")
        mkdir -p "$SDCARD/$group/$name"
        "${RSYNC[@]}" "$src" "$SDCARD/$group/$name/"
        ok "$group/$name"
    done
    for f in "$BUILD/$group"/*.json; do
        [ -f "$f" ] || continue
        "${RSYNC[@]}" "$f" "$SDCARD/$group/"
        ok "$group/$(basename "$f")"
    done
done

sync 2>/dev/null || true
printf "${GREEN}Deployed.${RESET} Eject the card and boot the Pocket.\n"
