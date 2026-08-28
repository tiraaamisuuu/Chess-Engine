[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Root,

    [Parameter(Mandatory = $true)]
    [string]$EnvironmentFile
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$binaryUrl = 'https://www.sfml-dev.org/files/SFML-2.6.2-windows-vc17-64-bit.zip'
$binarySha256 = 'F5995724604D74D06EFE89544315D11AC2D3C2D348854C6BE55A7A11FB7ECAD8'
$sourceUrl = 'https://github.com/SFML/SFML/archive/refs/tags/2.6.2.zip'
$sourceSha256 = '19D6DBD9C901C74441D9888C13CB1399F614FE8993D59062A72CFBCEB00FED04'

if (Test-Path -LiteralPath $Root) {
    throw "SFML output root already exists: $Root"
}

New-Item -ItemType Directory -Path $Root | Out-Null
$binaryArchive = Join-Path $Root 'SFML-2.6.2-windows-vc17-64-bit.zip'
$binaryExtract = Join-Path $Root 'binary'
$sfmlDir = $null

Write-Host 'Trying the official prebuilt SFML 2.6.2 archive...'
& curl.exe --fail --location --connect-timeout 15 --max-time 120 `
    --retry 2 --retry-all-errors --retry-delay 5 `
    --output $binaryArchive $binaryUrl
$binaryDownloaded = $LASTEXITCODE -eq 0

if ($binaryDownloaded) {
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $binaryArchive).Hash
    if ($actual -eq $binarySha256) {
        Expand-Archive -LiteralPath $binaryArchive -DestinationPath $binaryExtract
        $binaryRoot = Get-ChildItem -LiteralPath $binaryExtract -Directory |
            Select-Object -First 1
        if (-not $binaryRoot) {
            throw 'The official SFML binary archive did not contain a root directory.'
        }
        $sfmlDir = Join-Path $binaryRoot.FullName 'lib\cmake\SFML'
    } else {
        Write-Warning "Official SFML binary checksum mismatch: $actual"
    }
} else {
    Write-Warning 'The official SFML binary host was unavailable.'
}

if (-not $sfmlDir) {
    Write-Host 'Falling back to the pinned SFML 2.6.2 source archive on GitHub...'
    $sourceArchive = Join-Path $Root 'SFML-2.6.2-source.zip'
    $sourceExtract = Join-Path $Root 'source'
    $sourceBuild = Join-Path $Root 'source-build'
    $sourceInstall = Join-Path $Root 'install'

    & curl.exe --fail --location --connect-timeout 15 --max-time 180 `
        --retry 4 --retry-all-errors --retry-delay 5 `
        --output $sourceArchive $sourceUrl
    if ($LASTEXITCODE -ne 0) {
        throw 'The pinned SFML source archive download failed after retries.'
    }

    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceArchive).Hash
    if ($actual -ne $sourceSha256) {
        throw "SFML source checksum mismatch: $actual"
    }

    Expand-Archive -LiteralPath $sourceArchive -DestinationPath $sourceExtract
    $sourceRoot = Get-ChildItem -LiteralPath $sourceExtract -Directory |
        Select-Object -First 1
    if (-not $sourceRoot) {
        throw 'The SFML source archive did not contain a root directory.'
    }

    & cmake -S $sourceRoot.FullName -B $sourceBuild `
        -G 'Visual Studio 17 2022' -A x64 `
        "-DCMAKE_INSTALL_PREFIX=$sourceInstall" `
        -DSFML_BUILD_EXAMPLES=OFF -DSFML_BUILD_DOC=OFF -DSFML_BUILD_NETWORK=OFF
    if ($LASTEXITCODE -ne 0) { throw 'SFML source configuration failed.' }

    & cmake --build $sourceBuild --config Release --target install --parallel
    if ($LASTEXITCODE -ne 0) { throw 'SFML source build failed.' }

    $sfmlDir = Join-Path $sourceInstall 'lib\cmake\SFML'
}

$config = Join-Path $sfmlDir 'SFMLConfig.cmake'
if (-not (Test-Path -LiteralPath $config)) {
    throw "SFMLConfig.cmake was not found under $sfmlDir"
}

"SFML_DIR=$sfmlDir" | Out-File -FilePath $EnvironmentFile -Append
Write-Host "SFML_DIR=$sfmlDir"
