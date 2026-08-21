param(
    [Parameter(Position = 0)]
    [string]$ToolsBin = ".\build\Release\chess-engine-tools.exe",
    [Parameter(Position = 1)]
    [string]$UciBin = ".\build\Release\chess-engine-uci.exe",
    [int]$PerftDepth = 4,
    [int]$BenchDepth = 6,
    [int]$BenchTimeMs = 1500,
    [int]$BenchTtMb = 128
)

$ErrorActionPreference = "Stop"

foreach ($binary in @($ToolsBin, $UciBin)) {
    if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
        throw "Required binary not found: $binary"
    }
}

$ToolsBin = (Resolve-Path -LiteralPath $ToolsBin).Path
$UciBin = (Resolve-Path -LiteralPath $UciBin).Path
$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Output "== Perft Regression =="
& $ToolsBin --perft-tests --max-depth $PerftDepth
if ($LASTEXITCODE -ne 0) {
    throw "Perft regression failed with code $LASTEXITCODE"
}

Write-Output ""
Write-Output "== Search Benchmark =="
& $ToolsBin --bench `
    --bench-depth $BenchDepth `
    --bench-time $BenchTimeMs `
    --bench-tt $BenchTtMb
if ($LASTEXITCODE -ne 0) {
    throw "Search benchmark failed with code $LASTEXITCODE"
}

Write-Output ""
Write-Output "== UCI Smoke =="
& (Join-Path $ScriptRoot "run_uci_smoke.ps1") $UciBin

Write-Output ""
Write-Output "Quality gate: PASS"
