#!/usr/bin/env bash
#
# fix-scad-trailing-comma.sh
#
# Fixes a known scad-clj code generation bug where vector literals
# have a trailing comma with a missing element:
#   [1, 0, ]  →  [1, 0, 0]
#   [4.875, -4.15, ]  →  [4.875, -4.15, 0]
#
# The missing element is always the Z component (intended as 0).
# OpenSCAD silently treats it as undef→0 today, but this is
# technically malformed and may break in future versions.
#
# Usage:
#   ./scripts/fix-scad-trailing-comma.sh [file_or_dir ...]
#
# If no arguments given, defaults to things/*.scad in the repo root.
# Operates in-place. Safe to run multiple times (idempotent).

set -euo pipefail

fix_file() {
    local f="$1"
    local count
    count=$(grep -cP ',\s*\]' "$f" || true)
    if [ "$count" -gt 0 ]; then
        sed -i -E 's/,\s*\]/, 0]/g' "$f"
        echo "  fixed $count trailing-comma vectors in $f"
    fi
}

targets=("$@")
if [ ${#targets[@]} -eq 0 ]; then
    # Default: all .scad files under things/
    script_dir="$(cd "$(dirname "$0")" && pwd)"
    repo_root="$(cd "$script_dir/.." && pwd)"
    shopt -s nullglob
    targets=("$repo_root"/things/*.scad)
    if [ ${#targets[@]} -eq 0 ]; then
        echo "No .scad files found in $repo_root/things/"
        exit 1
    fi
fi

total=0
for f in "${targets[@]}"; do
    if [ ! -f "$f" ]; then
        echo "  skipping $f (not a file)"
        continue
    fi
    before=$(grep -cP ',\s*\]' "$f" || true)
    fix_file "$f"
    total=$((total + before))
done

echo "Done. Fixed $total trailing-comma vectors total."
