# SPDX-License-Identifier: MIT

# Stages the portable Windows package (dist/RemindMe-<version>-windows-portable[.zip])
# from a release build. RemindMe.exe is statically linked, so it is the only binary.

param(
    [string]$RepoRoot = (Resolve-Path ".").Path,
    [string]$Version = "",
    [string]$ReleaseExe = "",
    [string]$DistDir = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
Set-Location $RepoRoot

$appName = "RemindMe"
if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = [regex]::Match((Get-Content -Raw "Cargo.toml"), '(?m)^version\s*=\s*"([^"]+)"').Groups[1].Value
}
if ([string]::IsNullOrWhiteSpace($ReleaseExe)) {
    $ReleaseExe = Join-Path $RepoRoot "target/release/$appName.exe"
}
if ([string]::IsNullOrWhiteSpace($DistDir)) {
    $DistDir = Join-Path $RepoRoot "dist"
}
if (-not (Test-Path $ReleaseExe)) {
    throw "Release binary not found: $ReleaseExe`nRun: cargo build --release"
}

$packageBaseName = "{0}-{1}-windows-portable" -f $appName, $Version
$stageDir = Join-Path $DistDir $packageBaseName
$zipPath = Join-Path $DistDir ("{0}.zip" -f $packageBaseName)
Remove-Item -Path $stageDir, $zipPath -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $stageDir -Force | Out-Null

Copy-Item -Path $ReleaseExe -Destination (Join-Path $stageDir "$appName.exe") -Force
foreach ($file in @("README.md", "CHANGELOG.md", "LICENSE", "greetings.txt")) {
    if (Test-Path $file) {
        Copy-Item -Path $file -Destination (Join-Path $stageDir $file) -Force
    }
}

Compress-Archive -Path (Join-Path $stageDir "*") -DestinationPath $zipPath -Force
Write-Host "Portable release folder: $stageDir" -ForegroundColor Green
Write-Host "Portable release zip: $zipPath" -ForegroundColor Green
