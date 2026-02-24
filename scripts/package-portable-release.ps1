# SPDX-License-Identifier: MIT

param(
    [string]$RepoRoot = (Resolve-Path ".").Path,
    [string]$Version = "",
    [string]$ReleaseBuildDir = "",
    [string]$DistDir = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
Set-Location $RepoRoot

function Get-MatchValue {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Label
    )

    if (-not (Test-Path $Path)) {
        throw "$Label source file not found: $Path"
    }

    $content = Get-Content -Raw -Path $Path
    $match = [regex]::Match($content, $Pattern)
    if (-not $match.Success) {
        throw "Could not parse $Label from $Path"
    }

    return $match.Groups[1].Value
}

if ([string]::IsNullOrWhiteSpace($ReleaseBuildDir)) {
    $ReleaseBuildDir = Join-Path $RepoRoot "build/release"
}

if ([string]::IsNullOrWhiteSpace($DistDir)) {
    $DistDir = Join-Path $RepoRoot "dist"
}

$appName = Get-MatchValue -Path "CMakeLists.txt" -Pattern 'set\(APP_NAME\s+"([^"]+)"\)' -Label "APP_NAME"

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = Get-MatchValue -Path "include/remindme/app_info.hpp" -Pattern 'kAppVersion\s*=\s*"([^"]+)"' -Label "app version"
}

$releaseExe = Join-Path $ReleaseBuildDir ("{0}.exe" -f $appName)
if (-not (Test-Path $releaseExe)) {
    throw "Release binary not found: $releaseExe`nRun: cmake --preset release && cmake --build --preset release --parallel"
}

$packageBaseName = "{0}-{1}-windows-portable" -f $appName, $Version
$stageDir = Join-Path $DistDir $packageBaseName
$zipPath = Join-Path $DistDir ("{0}.zip" -f $packageBaseName)

if (Test-Path $stageDir) {
    Remove-Item -Path $stageDir -Recurse -Force
}
if (Test-Path $zipPath) {
    Remove-Item -Path $zipPath -Force
}

New-Item -ItemType Directory -Path $stageDir -Force | Out-Null
Copy-Item -Path $releaseExe -Destination (Join-Path $stageDir ("{0}.exe" -f $appName)) -Force

$extraFiles = @(
    "README.md",
    "CHANGELOG.md",
    "LICENSE",
    "greetings.txt"
)
foreach ($file in $extraFiles) {
    if (Test-Path $file) {
        Copy-Item -Path $file -Destination (Join-Path $stageDir $file) -Force
    }
}

$windeployqtCommand = Get-Command -Name windeployqt6.exe, windeployqt.exe -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $windeployqtCommand) {
    throw "windeployqt was not found on PATH. Expected MSYS2 Qt tools in PATH."
}

$stagedExe = Join-Path $stageDir ("{0}.exe" -f $appName)
& $windeployqtCommand.Source --release --force $stagedExe
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

Compress-Archive -Path (Join-Path $stageDir "*") -DestinationPath $zipPath -Force

Write-Host "Portable release folder: $stageDir" -ForegroundColor Green
Write-Host "Portable release zip: $zipPath" -ForegroundColor Green
