#!/usr/bin/env bash
# Known-good InfiniSim launcher for InfiniTime---Personal
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="${INFINSIM_BUILD:-/home/dethbox/Projects/build_lv_sim}"
export DISPLAY="${DISPLAY:-:0}"
# Prefer X11 backend under Wayland — more reliable for SDL2 + mouse button wake
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}"

if [[ ! -x "$BUILD/infinisim" ]]; then
  echo "Missing $BUILD/infinisim — build first (see tools/infinisim-patches/README.md)" >&2
  exit 1
fi

cd "$BUILD"
# Drop stale instances so you never run an old binary by mistake
pkill -f "$BUILD/infinisim" 2>/dev/null || true
sleep 0.2
exec ./infinisim --hide-status "$@"
