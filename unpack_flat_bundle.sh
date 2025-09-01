#!/usr/bin/env bash
# unpack_flat_bundle.sh
# Recreate files from a delimited flat text bundle produced by make_flat_bundle.sh
# Usage: unpack_flat_bundle.sh <BUNDLE_FILE> <DEST_DIR>

set -euo pipefail

BUNDLE="${1:?Bundle file required}"
DEST="${2:?Destination directory required}"

mkdir -p "$DEST"

current=""
mode=""
while IFS= read -r line || [ -n "$line" ]; do
  if [[ "$line" == =====FILE\|* ]]; then
    # Start a new file
    # Format: =====FILE|<rel>|mode=####|size=N|mtime=ISO|sha256=...
    current=$(printf "%s\n" "$line" | sed -E 's/^=====FILE\|([^|]+)\|.*$/\1/')
    mode=$(printf "%s\n" "$line" | sed -E 's/.*\|mode=([0-9]+).*/\1/')
    out="$DEST/$current"
    mkdir -p "$(dirname "$out")"
    : > "$out"
    continue
  fi
  if [[ "$line" == "=====END FILE=====" ]]; then
    # Apply mode if present
    if [[ -n "${mode:-}" ]]; then
      chmod "$mode" "$out" 2>/dev/null || true
    fi
    current=""
    mode=""
    continue
  fi
  if [[ -n "$current" ]]; then
    printf "%s\n" "$line" >> "$out"
  fi
done < "$BUNDLE"

echo "✅ Unpacked into: $DEST"
