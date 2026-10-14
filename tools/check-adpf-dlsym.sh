#!/usr/bin/env bash
# Brief 004 offline native check: the compositor's ADPF code must not link APerformanceHint_*
# (NDK API 33) because minSdk is 26; it resolves them with dlsym. Usage:
#   tools/check-adpf-dlsym.sh            compile adpf.c for android26 with availability errors on
#   tools/check-adpf-dlsym.sh <lib.so>   also check a built libdroiddeckwayland.so has no such imports
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
ndk="${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/27.3.13750724}"
bin="$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin"
src="$here/app/src/main/cpp/waylandcomp/src"
"$bin/aarch64-linux-android26-clang" -c -Wall -Werror -Werror=unguarded-availability -I"$src" "$src/adpf.c" -o /dev/null
echo "adpf.c: compiles for android26 (no direct APerformanceHint use)"
if [[ $# -ge 1 ]]; then
    if "$bin/llvm-nm" -D --undefined-only "$1" | grep -q APerformanceHint_; then
        echo "FAIL: $1 imports APerformanceHint_* directly" >&2; exit 1
    fi
    "$bin/llvm-strings" "$1" | grep -q APerformanceHint_createSession || { echo "FAIL: dlsym name missing from $1" >&2; exit 1; }
    echo "$(basename "$1"): APerformanceHint_* resolved via dlsym only"
fi
