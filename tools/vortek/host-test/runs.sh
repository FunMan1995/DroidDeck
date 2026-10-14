#!/usr/bin/env bash
# Brief 003 §4.3 runs (a)-(f) on the x86_64 host harness. No device. Every result is
# "lavapipe + Pixel 9a profile, not komodo".
# usage: runs.sh <a|b|c|d|e|f> <out_dir> [harness_dir]     (harness_dir from build.sh; for (c) a baseline build)
#   a  lavapipe direct              b  lavapipe+50720 direct
#   c  lavapipe+50720 via Vortek (upstream, no shims; pass a baseline harness_dir)
#   d  same via Vortek with the DroidDeck shims    e  feature_test with server-side validation
#   f  soak: bctest.exe in a loop under Vortek for SOAK_SECS (600), sampling server fd count + RSS
set -uo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
run=${1:?run a-f}; out=${2:?out dir}; hh=${3:-/tmp/hh}
bc=${BCTEST:-/workspace/dxvk-bc/bctest.exe}; wine=${WINE:-$HOME/.local/opt/wine-11.19-amd64-wow64/bin/wine}
penv=${PROFILE_ENV:-/workspace/briefs/mali-profile-env.sh}; vlayers=${VVL_PATH:-/home/box/Android/Sdk/emulator/lib64/vulkan/layers}
mkdir -p "$out"; cp "$bc" "$out/"; cd "$out"
for v in $(env | grep -oE '^VK_[A-Z_]+'); do unset "$v"; done
label="lavapipe + Pixel 9a profile, not komodo"

wine_run() {  # wine bctest.exe once; DXVK logs land in $out
    WINEPREFIX=$HOME/.local/share/dd-wine WINEDEBUG=-all WINEDLLOVERRIDES="d3d11,dxgi=n;mscoree,mshtml=" \
    DXVK_LOG_LEVEL=debug DXVK_HUD=0 DXVK_LOG_PATH="$out" timeout 120 xvfb-run -a "$wine" bctest.exe >> out.txt 2>&1
}
verdict() { local r; r=$(grep -m1 -o "Skipping: .*" bctest_dxgi.log 2>/dev/null) || r="no device rejected"
    grep -q "D3D11CreateDevice hr=0x0" out.txt && r="device OK ($r)"; echo "($run) $label: $r"; }
start_server() {  # $1 = extra layers ("" or validation)
    sock="$out/vortek.sock"; rm -f "$sock"
    ( set -a; . "$penv"; set +a; export VK_KHRONOS_PROFILES_EMULATE_PORTABILITY=false
      [[ -n "$1" ]] && export VK_LAYER_PATH="$VK_LAYER_PATH:$vlayers" VK_INSTANCE_LAYERS="VK_LAYER_KHRONOS_validation:$VK_INSTANCE_LAYERS"
      exec env VORTEK_SERVER_PATH="$sock" "$hh/vortek-host-server" ) > server.log 2>&1 &
    spid=$!; for _ in $(seq 50); do [[ -S "$sock" ]] && break; sleep 0.1; done
    export VK_ICD_FILENAMES="$hh/vortek_icd.x86_64.json" VORTEK_SERVER_PATH="$sock"
}
stop_server() { kill "$spid" 2>/dev/null; wait "$spid" 2>/dev/null; }

case $run in
a) export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json; wine_run; verdict ;;
b) set -a; . "$penv"; set +a; wine_run; verdict ;;
c|d) start_server ""; wine_run; stop_server; verdict ;;
e) start_server validation; "$hh/feature_test" > out.txt 2>&1; rc=$?; stop_server
   n=$(grep -c "Validation Error" server.log); echo "(e) $label: feature_test rc=$rc, validation errors=$n"
   grep -o "Validation Error: \[ [A-Za-z0-9_-]* \]" server.log | sort | uniq -c ;;
f) secs=${SOAK_SECS:-600}; start_server ""; t0=$SECONDS; i=0; echo "t_s iter fds rss_kb" > soak.tsv
   while (( SECONDS - t0 < secs )); do wine_run; i=$((i+1))
       echo "$((SECONDS-t0)) $i $(ls /proc/$spid/fd | wc -l) $(awk '/VmRSS/{print $2}' /proc/$spid/status)" >> soak.tsv
   done; stop_server
   echo "(f) $label: $i iterations in $((SECONDS-t0))s; first/last sample:"; sed -n '2p;$p' soak.tsv ;;
*) echo "unknown run $run" >&2; exit 2 ;;
esac
