param(
    [string]$BuildDir = "",
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $BuildDir) {
    $BuildDir = Join-Path $Root "build-pc-max"
} elseif (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $Root $BuildDir
}

cmake -S $Root -B $BuildDir `
    -DCHESS_BUILD_GUI=OFF `
    -DBUILD_TESTING=ON `
    -DCHESS_ENABLE_IPO=ON `
    -DCHESS_NATIVE_ARCH=ON
if ($LASTEXITCODE -ne 0) {
    throw "Maximum-performance CMake configuration failed."
}

cmake --build $BuildDir --config Release --parallel
if ($LASTEXITCODE -ne 0) {
    throw "Maximum-performance build failed."
}

if (-not $SkipTests) {
    ctest --test-dir $BuildDir -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "Maximum-performance tests failed."
    }
}

$Engine = Join-Path $BuildDir "Release\chess-engine-uci.exe"
if (-not (Test-Path $Engine)) {
    $Engine = Join-Path $BuildDir "chess-engine-uci.exe"
}
if (-not (Test-Path $Engine)) {
    throw "Maximum-performance engine executable was not produced."
}

Write-Host "Maximum-performance engine: $Engine"
Write-Host "This binary targets the build PC and is not intended as a portable release."
