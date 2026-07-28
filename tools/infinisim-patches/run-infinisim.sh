#!/usr/bin/env bash
# Known-good InfiniSim launcher for InfiniTime---Personal
set -euo pipefail
BUILD="${INFINSIM_BUILD:-/home/dethbox/Projects/build_lv_sim}"
BIN="$BUILD/infinisim"
export DISPLAY="${DISPLAY:-:0}"
# X11 via XWayland is more reliable than native Wayland for this SDL2 build
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}"

if [[ ! -x "$BIN" ]]; then
  echo "Missing $BIN — build first (see tools/infinisim-patches/README.md)" >&2
  exit 1
fi

cd "$BUILD"
# Drop stale instances so you never run an old binary by mistake
pkill -f "$BIN" 2>/dev/null || true
sleep 0.3
echo "Starting $BIN (mtime $(stat -c %y "$BIN" | cut -d. -f1))"
exec "$BIN" --hide-status "$@"
