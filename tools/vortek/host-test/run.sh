#!/usr/bin/env bash
# Builds and runs the batch-1 shim unit tests on the host (x86_64, gcc). No device, no driver.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"; v="$here/.."; out="$(mktemp -d)"; trap 'rm -rf "$out"' EXIT
gcc -std=gnu11 -O0 -g -w -I"$v/server/droiddeck" -I"$v/server/vortekrenderer/include" -I"$v/server/winlator/include" \
    "$here/test_shims.c" "$v/server/droiddeck/droiddeck_shims.c" -o "$out/test_shims" -lpthread
"$out/test_shims"
