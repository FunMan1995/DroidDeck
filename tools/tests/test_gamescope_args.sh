#!/bin/bash
# Brief 005: droiddeck-session's gamescope argument builder for komodo presets (host-only, no guest).
set -u
here=$(cd "$(dirname "$0")" && pwd)
script=$here/../linuxfs/overlay/usr/local/bin/droiddeck-session
fail=0
load() { eval "$(sed -n '/^# >>> gamescope_args/,/^# <<< gamescope_args/p' "$script")"; }
load
declare -F gamescope_args >/dev/null || { echo "FAIL: gamescope_args not found in $script"; exit 1; }

check() { # name expected-args... ; env already set by caller
  local name=$1; shift
  local want="$*" got
  args=(); gamescope_args >/dev/null
  got="${args[*]}"
  if [ "$got" = "$want" ]; then echo "ok   $name"; else echo "FAIL $name"; echo "  want: $want"; echo "  got:  $got"; fail=1; fi
}
base() { unset BL_FPS BL_REFRESH BL_FOLLOW_OUTPUT BL_GAMESCOPE_STRETCH_16X9 BL_STEAM_UI BL_XWAYLAND_COUNT BL_HDR BL_GAMESCOPE_ARGS BL_STRETCHED; }
mangoapp() { :; }  # keep host mangoapp out of it

base; mode=run BL_WIDTH=1496 BL_HEIGHT=672 BL_REFRESH=120
check "panel/2 run 120Hz" --backend wayland --expose-wayland -f -W 1496 -H 672 -w 1496 -h 672 -r 120

base; mode=steam BL_WIDTH=1496 BL_HEIGHT=672 BL_REFRESH=120
check "panel/2 steam 120Hz" --backend wayland --expose-wayland -f -W 1496 -H 672 -w 1496 -h 672 -r 120 -e --xwayland-count 2 --default-touch-mode 4

base; mode=run BL_WIDTH=1496 BL_HEIGHT=672 BL_REFRESH=120 BL_FPS=40
check "fps cap wins over refresh" --backend wayland --expose-wayland -f -W 1496 -H 672 -w 1496 -h 672 -r 40

base; mode=steam BL_WIDTH=1496 BL_HEIGHT=672 BL_REFRESH=120 BL_GAMESCOPE_STRETCH_16X9=1
check "16:9 stretch fits within 1496 (no stretch)" --backend wayland --expose-wayland -f -W 1496 -H 672 -w 1496 -h 672 -r 120 -e --xwayland-count 2 --default-touch-mode 4

base; mode=run BL_WIDTH=2244 BL_HEIGHT=1008 BL_REFRESH=60 BL_HDR=1
check "low-res mode 60Hz HDR" --backend wayland --expose-wayland -f -W 2244 -H 1008 -w 2244 -h 1008 -r 60 --hdr-enabled

exit $fail
