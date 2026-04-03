# save as run_dottalkpp.sh (in /mnt/d/code/ccode), then: chmod +x run_dottalkpp.sh
#!/usr/bin/env bash
set -euo pipefail

# repo root (this script's directory)
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$ROOT/build-wsl"
BIN="$BUILD/src/dottalkpp"
WORKDIR="$ROOT/data"

# sanity checks
[[ -x "$BIN" ]] || { echo "ERROR: $BIN not found or not executable. Build first."; exit 1; }
[[ -d "$WORKDIR" ]] || { echo "ERROR: Work dir $WORKDIR not found."; exit 1; }

# run with data/ as CWD
cd "$WORKDIR"
exec "$BIN" "$@"
