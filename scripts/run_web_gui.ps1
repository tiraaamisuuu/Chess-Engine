$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Build = if ($env:BUILD_DIR) { $env:BUILD_DIR } else { Join-Path $Root "build-web" }
$Venv = if ($env:WEB_VENV) { $env:WEB_VENV } else { Join-Path $Root ".venv-web" }
$Python = Join-Path $Venv "Scripts\python.exe"

if (-not (Test-Path $Python)) {
    $PythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($PythonCommand) {
        $PreviousErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        & $PythonCommand.Source -c "import sys; raise SystemExit(sys.version_info < (3, 10))"
        $PythonVersionExitCode = $LASTEXITCODE
        $ErrorActionPreference = $PreviousErrorActionPreference
        if ($PythonVersionExitCode -eq 0) {
            & $PythonCommand.Source -m venv $Venv
        } else {
            $PythonCommand = $null
        }
    }

    if (-not $PythonCommand) {
        $PyLauncher = Get-Command py.exe -ErrorAction SilentlyContinue
        if (-not $PyLauncher) {
            throw "Python 3.10 or newer was not found. Install Python and retry."
        }
        & $PyLauncher.Source -3 -m venv $Venv
    }

    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $Python)) {
        throw "Failed to create the web Python environment: $Venv"
    }
}

$PreviousErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"
& $Python -c "import chess" 2>$null
$ChessImportExitCode = $LASTEXITCODE
$ErrorActionPreference = $PreviousErrorActionPreference
if ($ChessImportExitCode -ne 0) {
    & $Python -m pip install --disable-pip-version-check -r (Join-Path $Root "web\requirements.txt")
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to install the web Python dependencies."
    }
}

$PackagedEngine = Join-Path $Root "bin\chess-engine-uci.exe"
$Engine = if ($env:ENGINE_BIN) {
    $env:ENGINE_BIN
} elseif (Test-Path $PackagedEngine) {
    $PackagedEngine
} else {
    Join-Path $Build "Release\chess-engine-uci.exe"
}
if (-not (Test-Path $Engine)) {
    if (-not (Test-Path (Join-Path $Root "CMakeLists.txt"))) {
        throw "The packaged UCI engine is missing: $Engine"
    }
    cmake -S $Root -B $Build -DCHESS_BUILD_GUI=OFF -DBUILD_TESTING=OFF
    cmake --build $Build --config Release --parallel
}
if (-not (Test-Path $Engine)) {
    $Engine = Join-Path $Build "chess-engine-uci.exe"
}
if (-not (Test-Path $Engine)) {
    throw "The chess engine UCI executable was not produced."
}

& $Python (Join-Path $Root "web\server.py") --engine $Engine @args
