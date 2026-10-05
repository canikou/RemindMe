# SPDX-License-Identifier: MIT

param(
    [string]$RepoRoot = (Resolve-Path ".").Path,
    [string]$Version = "",
    [string]$PortableDir = "",
    [string]$DistDir = "",
    [string]$InnoCompiler = ""
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

function Resolve-InnoCompiler {
    param(
        [string]$PreferredPath
    )

    if (-not [string]::IsNullOrWhiteSpace($PreferredPath) -and (Test-Path $PreferredPath)) {
        return (Resolve-Path $PreferredPath).Path
    }

    $candidates = @()

    $fromPath = Get-Command -Name iscc.exe, iscc -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($fromPath) {
        $candidates += $fromPath.Source
    }

    $candidates += @(
        (Join-Path $env:LOCALAPPDATA "Programs\\Inno Setup 6\\ISCC.exe"),
        (Join-Path $env:ProgramFiles "Inno Setup 6\\ISCC.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\\ISCC.exe")
    )

    foreach ($candidate in $candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw @"
Inno Setup compiler (ISCC.exe) not found.
Install it with:
  winget install --id JRSoftware.InnoSetup -e --source winget
"@
}

$appName = Get-MatchValue -Path "CMakeLists.txt" -Pattern 'set\(APP_NAME\s+"([^"]+)"\)' -Label "APP_NAME"

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = Get-MatchValue -Path "include/remindme/app_info.hpp" -Pattern 'kAppVersion\s*=\s*"([^"]+)"' -Label "app version"
}

if ([string]::IsNullOrWhiteSpace($DistDir)) {
    $DistDir = Join-Path $RepoRoot "dist"
}

if ([string]::IsNullOrWhiteSpace($PortableDir)) {
    $PortableDir = Join-Path $DistDir ("{0}-{1}-windows-portable" -f $appName, $Version)
}

if (-not (Test-Path $PortableDir)) {
    throw "Portable package folder not found: $PortableDir`nRun scripts/package-portable-release.ps1 first."
}

$issPath = Join-Path $RepoRoot "installer/RemindMe.iss"
if (-not (Test-Path $issPath)) {
    throw "Installer script not found: $issPath"
}

$iscc = Resolve-InnoCompiler -PreferredPath $InnoCompiler

New-Item -ItemType Directory -Path $DistDir -Force | Out-Null
$resolvedDistDir = (Resolve-Path $DistDir).Path

& $iscc `
    "/Qp" `
    "/O$resolvedDistDir" `
    "/DMyAppVersion=$Version" `
    "/DMySourceDir=$PortableDir" `
    $issPath

if ($LASTEXITCODE -ne 0) {
    throw "ISCC failed with exit code $LASTEXITCODE"
}

$setupPath = Join-Path $DistDir ("{0}-{1}-setup.exe" -f $appName, $Version)
if (-not (Test-Path $setupPath)) {
    throw "Expected setup output missing: $setupPath"
}

Write-Host "Setup installer: $setupPath" -ForegroundColor Green
