#!/usr/bin/env bash
# make_flat_bundle.sh
# Create a single delimited flat text file containing project source + script files.
# Usage: make_flat_bundle.sh [PROJECT_ROOT] [OUTPUT_FILE]
# Example: make_flat_bundle.sh /mnt/c/Users/deral/code/ccode sources_bundle.txt

set -euo pipefail

ROOT="${1:-$(pwd)}"
OUT="${2:-sources_bundle.txt}"

# Directories to exclude (regex for find -path)
EXCLUDE_DIRS=(
  ".git"
  ".vs"
  ".vscode"
  "build"
  "dist"
  "out"
  "results"
  "__pycache__"
  "node_modules"
  ".idea"
)

# File name patterns to include (add more if you like)
INCLUDE_NAMES=(
  "*.c" "*.h" "*.hpp" "*.hh" "*.hxx"
  "*.cc" "*.cpp" "*.cxx" "*.ipp" "*.inl"
  "CMakeLists.txt" "*.cmake"
  "*.ps1" "*.bat" "*.cmd" "*.sh" "*.bash"
  "*.py" "*.pl"
  "*.json" "*.yml" "*.yaml" "*.toml" "*.ini" ".editorconfig"
  "*.md" "*.txt" "*.csv"
  "Makefile" "makefile"
)

# Build find predicates
build_find_predicate() {
  local expr=""
  # Exclude directories
  for d in "${EXCLUDE_DIRS[@]}"; do
    expr+=" -path \"$ROOT/**/$d\" -prune -o"
  done
  # Include name patterns
  expr+=" ( -type f ("
  local first=1
  for n in "${INCLUDE_NAMES[@]}"; do
    if [ $first -eq 1 ]; then
      expr+=" -name \"$n\""
      first=0
    else
      expr+=" -o -name \"$n\""
    fi
  done
  expr+=" ) ) -print"
  printf "%s" "$expr"
}

FIND_EXPR=$(build_find_predicate)

# Normalize path printing (GNU stat & sha256sum expected)
if ! command -v sha256sum >/dev/null 2>&1; then
  echo "ERROR: sha256sum not found. Install coreutils or add sha256sum to PATH." >&2
  exit 1
fi

# Create/clear output
: > "$OUT"

# Header note
{
  echo "### SOURCES BUNDLE"
  echo "### root=$ROOT"
  echo "### generated=$(date -u +'%Y-%m-%dT%H:%M:%SZ')"
  echo
} >> "$OUT"

# Iterate files
# shellcheck disable=SC2086
eval find \""$ROOT"\" $FIND_EXPR | while IFS= read -r f; do
  # Relpath
  rel="${f#$ROOT/}"
  # If file is exactly ROOT, keep as "."
  [ "$rel" = "$f" ] && rel="."

  # Metadata
  # - use 4-digit mode to preserve leading 0 (use %a for numeric, %A for text)
  mode=$(stat -c '%a' "$f")
  size=$(stat -c '%s' "$f")
  mtime=$(stat -c '%y' "$f" | sed 's/ /T/; s/\..*$/Z/')
  hash=$(sha256sum "$f" | awk '{print $1}')

  # Delimited entry
  {
    echo "=====FILE|$rel|mode=$mode|size=$size|mtime=$mtime|sha256=$hash====="
    cat "$f"
    # Ensure trailing newline so the end marker is on a new line
    [ -s "$f" ] && [ "$(tail -c1 "$f" | wc -l)" -eq 0 ] && echo
    echo "=====END FILE====="
  } >> "$OUT"
done

echo "✅ Wrote bundle: $OUT"
