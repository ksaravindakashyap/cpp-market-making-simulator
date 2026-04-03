# Start the WebSocket server with the long demo CSV so the dashboard has time to connect
# and see trades, PnL, and other channels. Build mmsim_ws_server first.
#
# Terminal 1: .\scripts\run-demo.ps1
# Terminal 2: cd dashboard; npm install; npm run dev
# Terminal 3: cd dashboard; npm run verify:ws   (optional automated check)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$Exe = Join-Path $Root "build_mingw\server\mmsim_ws_server.exe"
if (-not (Test-Path $Exe)) {
    Write-Host "mmsim_ws_server not found at $Exe"
    Write-Host "Build with: cmake -B build_mingw -G `"MinGW Makefiles`" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++"
    Write-Host "          cmake --build build_mingw --target mmsim_ws_server -j"
    exit 1
}

$Data = Join-Path $Root "data\demo_ticks.csv"
if (-not (Test-Path $Data)) {
    Write-Host "Generating demo_ticks.csv..."
    node (Join-Path $Root "scripts\generate-demo-ticks.mjs")
}

Write-Host "Server: $Exe"
Write-Host "Data:   $Data (250 ticks, 10x replay)"
Write-Host "Open another terminal: cd dashboard; npm run dev -> http://localhost:5173"
Write-Host "Optional check:        cd dashboard; npm run verify:ws"
Write-Host ""

& $Exe --data $Data --port 8080 --speed 10x
