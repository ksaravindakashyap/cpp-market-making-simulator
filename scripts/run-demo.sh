#!/usr/bin/env bash
# Start mmsim_ws_server with data/demo_ticks.csv (Git Bash / Linux / macOS).
# Build the server first; adjust EXE path for your generator.

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

EXE=""
if [[ -x "$ROOT/build_mingw/server/mmsim_ws_server.exe" ]]; then
  EXE="$ROOT/build_mingw/server/mmsim_ws_server.exe"
elif [[ -x "$ROOT/build/server/mmsim_ws_server" ]]; then
  EXE="$ROOT/build/server/mmsim_ws_server"
else
  echo "mmsim_ws_server not found. Build it (see README) then re-run."
  exit 1
fi

DATA="$ROOT/data/demo_ticks.csv"
if [[ ! -f "$DATA" ]]; then
  node "$ROOT/scripts/generate-demo-ticks.mjs"
fi

echo "Server: $EXE"
echo "Data:   $DATA"
echo "Other terminal: cd dashboard && npm run dev"
echo "Optional:         cd dashboard && npm run verify:ws"
echo ""

exec "$EXE" --data "$DATA" --port 8080 --speed 10x
