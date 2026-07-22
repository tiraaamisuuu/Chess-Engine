$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Destination = Join-Path $Root ".tools\cutechess-windows"
$Archive = Join-Path $Destination "cutechess-1.5.1-win64.zip"
$Url = "https://github.com/cutechess/cutechess/releases/download/v1.5.1/cutechess-1.5.1-win64.zip"
$ExpectedSha256 = "048942CA3473DB860CB914FE94108DA37051F693F47E368688EC6EC450F924BC"

$Existing = Get-ChildItem -Path $Destination -Filter "cutechess-cli.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if ($Existing) {
    Write-Host "Cute Chess CLI is ready: $($Existing.FullName)"
    exit 0
}

New-Item -ItemType Directory -Force -Path $Destination | Out-Null
Invoke-WebRequest -Uri $Url -OutFile $Archive
$ActualSha256 = (Get-FileHash -Path $Archive -Algorithm SHA256).Hash
if ($ActualSha256 -ne $ExpectedSha256) {
    throw "Cute Chess archive checksum mismatch."
}

Expand-Archive -Path $Archive -DestinationPath $Destination -Force
$Executable = Get-ChildItem -Path $Destination -Filter "cutechess-cli.exe" -Recurse | Select-Object -First 1
if (-not $Executable) {
    throw "cutechess-cli.exe was not present in the official archive."
}
Write-Host "Cute Chess CLI is ready: $($Executable.FullName)"
