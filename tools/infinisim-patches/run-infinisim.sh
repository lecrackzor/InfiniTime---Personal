#!/usr/bin/env bash
# InfiniSim launcher for InfiniTime---Personal (WSL / Linux)
set -euo pipefail
BUILD="${INFINSIM_BUILD:-$HOME/Projects/InfiniSim/build}"
BIN="$BUILD/infinisim"

export MAMBA_ROOT_PREFIX="${MAMBA_ROOT_PREFIX:-$HOME/nrf52/tools/micromamba/root}"
ENV="${MAMBA_ROOT_PREFIX}/envs/infinisim"
if [[ -d "$ENV/lib" ]]; then
  export LD_LIBRARY_PATH="$ENV/lib:${LD_LIBRARY_PATH:-}"
fi

# WSLg / X11: software GL is more reliable than indirect GLX
unset LIBGL_ALWAYS_INDIRECT || true
export LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-1}"
export GALLIUM_DRIVER="${GALLIUM_DRIVER:-llvmpipe}"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}"
export SDL_RENDER_DRIVER="${SDL_RENDER_DRIVER:-software}"
export DISPLAY="${DISPLAY:-:0}"

if [[ ! -x "$BIN" ]]; then
  echo "Missing $BIN — build InfiniSim first" >&2
  exit 1
fi

cd "$BUILD"
pkill -f "$BIN" 2>/dev/null || true
sleep 0.3
echo "Starting $BIN (mtime $(stat -c %y "$BIN" | cut -d. -f1))"
echo "Keys: 1 Digital, 2 Terminal, 3 Casio Custom, w weather, i screenshot"
exec "$BIN" --hide-status "$@"
