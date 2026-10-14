#!/usr/bin/env bash
# Regenerates patches/ (DroidDeck changes to the vendored upstream files) from git history:
# diff of client/ and server/vortekrenderer + server/winlator against the pristine import commit.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; cd "$here"
base=$(git log --format=%H --diff-filter=A -- PINS | tail -1)
mkdir -p patches
for half in client server; do
    paths=(client); [[ $half == server ]] && paths=(server/vortekrenderer server/winlator)
    { echo "DroidDeck changes to the vendored Vortek $half (LGPL-2.1), generated $(date -u +%Y-%m-%d)"
      echo "Base: pristine import $base (tools/vortek/PINS)"; echo
      git diff "$base" -- "${paths[@]}"; } > "patches/$half-droiddeck.patch"
done
