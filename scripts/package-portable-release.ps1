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

function Get-PeImportNames {
    param(
        [string]$Path
    )

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
    if ([BitConverter]::ToUInt32($bytes, $peOffset) -ne 0x4550) {
        throw "Not a PE file: $Path"
    }

    $sectionCount = [BitConverter]::ToUInt16($bytes, $peOffset + 6)
    $optionalHeaderSize = [BitConverter]::ToUInt16($bytes, $peOffset + 20)
    $optionalHeader = $peOffset + 24
    $isPe32Plus = [BitConverter]::ToUInt16($bytes, $optionalHeader) -eq 0x20B
    $dataDirectories = $optionalHeader + $(if ($isPe32Plus) { 112 } else { 96 })
    $importTableRva = [BitConverter]::ToUInt32($bytes, $dataDirectories + 8)
    if ($importTableRva -eq 0) {
        return @()
    }

    $sectionTable = $optionalHeader + $optionalHeaderSize
    $sections = for ($i = 0; $i -lt $sectionCount; $i++) {
        $entry = $sectionTable + ($i * 40)
        [pscustomobject]@{
            VirtualSize    = [BitConverter]::ToUInt32($bytes, $entry + 8)
            VirtualAddress = [BitConverter]::ToUInt32($bytes, $entry + 12)
            RawSize        = [BitConverter]::ToUInt32($bytes, $entry + 16)
            RawOffset      = [BitConverter]::ToUInt32($bytes, $entry + 20)
        }
    }

    $toFileOffset = {
        param([uint32]$Rva)
        foreach ($section in $sections) {
            $span = [Math]::Max($section.VirtualSize, $section.RawSize)
            if ($Rva -ge $section.VirtualAddress -and $Rva -lt ($section.VirtualAddress + $span)) {
                return [int]($Rva - $section.VirtualAddress + $section.RawOffset)
            }
        }
        throw "RVA 0x{0:X} is outside all sections in $Path" -f $Rva
    }

    $names = @()
    $descriptor = & $toFileOffset $importTableRva
    while ($true) {
        $nameRva = [BitConverter]::ToUInt32($bytes, $descriptor + 12)
        if ($nameRva -eq 0) {
            break
        }

        $nameStart = & $toFileOffset $nameRva
        $nameEnd = [Array]::IndexOf($bytes, [byte]0, $nameStart)
        $names += [System.Text.Encoding]::ASCII.GetString($bytes, $nameStart, $nameEnd - $nameStart)
        $descriptor += 20
    }

    return $names
}

function Copy-RuntimeDependencies {
    param(
        [string]$StageDir,
        [string[]]$SearchDirs
    )

    $systemDir = [Environment]::GetFolderPath([Environment+SpecialFolder]::System)
    $deployed = @{}
    Get-ChildItem -Path $StageDir -Recurse -File | ForEach-Object { $deployed[$_.Name.ToLowerInvariant()] = $true }

    $queue = New-Object System.Collections.Generic.Queue[string]
    Get-ChildItem -Path $StageDir -Recurse -File -Include *.exe, *.dll | ForEach-Object { $queue.Enqueue($_.FullName) }

    $copied = @()
    $unresolved = @{}
    while ($queue.Count -gt 0) {
        $binary = $queue.Dequeue()
        foreach ($dll in Get-PeImportNames -Path $binary) {
            $key = $dll.ToLowerInvariant()
            if ($deployed.ContainsKey($key) -or $key.StartsWith("api-ms-") -or $key.StartsWith("ext-ms-")) {
                continue
            }

            # Prefer the toolchain copy: some MinGW DLL names (e.g. zlib1.dll)
            # may also exist in System32 via third-party installers.
            $candidate = $SearchDirs | ForEach-Object { Join-Path $_ $dll } | Where-Object { Test-Path $_ } | Select-Object -First 1
            if ($candidate) {
                $destination = Join-Path $StageDir (Split-Path -Leaf $candidate)
                Copy-Item -Path $candidate -Destination $destination -Force
                $deployed[$key] = $true
                $copied += (Split-Path -Leaf $candidate)
                $queue.Enqueue($destination)
            } elseif (Test-Path (Join-Path $systemDir $dll)) {
                $deployed[$key] = $true
            } else {
                $unresolved[$dll] = (Split-Path -Leaf $binary)
            }
        }
    }

    if ($copied.Count -gt 0) {
        Write-Host ("Bundled runtime dependencies: {0}" -f (($copied | Sort-Object) -join ", "))
    }

    if ($unresolved.Count -gt 0) {
        $details = $unresolved.GetEnumerator() | Sort-Object Name | ForEach-Object { "  {0} (needed by {1})" -f $_.Name, $_.Value }
        throw ("Unresolved DLL dependencies in portable package:`n{0}" -f ($details -join "`n"))
    }
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

$windeployqtCommand = Get-Command -Name windeployqt6.exe, windeployqt-qt6.exe, windeployqt.exe -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $windeployqtCommand) {
    throw "windeployqt was not found on PATH. Expected MSYS2 Qt tools in PATH."
}

$stagedExe = Join-Path $stageDir ("{0}.exe" -f $appName)
& $windeployqtCommand.Source --release --force $stagedExe
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

# MSYS2's windeployqt only deploys Qt modules and plugins. The MinGW runtime
# (libstdc++, libgcc_s, libwinpthread) and Qt's third-party dependencies (ICU,
# PCRE2, zlib, zstd, harfbuzz, freetype, glib, ...) must be copied separately,
# otherwise the packaged app fails to start on machines without MSYS2 on PATH.
$dependencySearchDirs = @(
    Get-Command -Name Qt6Core.dll, g++.exe, $windeployqtCommand.Source -ErrorAction SilentlyContinue |
        ForEach-Object { Split-Path -Parent $_.Source }
) | Select-Object -Unique
Copy-RuntimeDependencies -StageDir $stageDir -SearchDirs $dependencySearchDirs

Compress-Archive -Path (Join-Path $stageDir "*") -DestinationPath $zipPath -Force

Write-Host "Portable release folder: $stageDir" -ForegroundColor Green
Write-Host "Portable release zip: $zipPath" -ForegroundColor Green
