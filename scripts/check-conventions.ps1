param(
    [string]$RepoRoot = (Resolve-Path ".").Path
)

$ErrorActionPreference = "Stop"
Set-Location $RepoRoot

$errors = New-Object System.Collections.Generic.List[string]

function Add-Err([string]$msg) {
    $errors.Add($msg)
}

function Assert-SnakeCaseFileName([string]$path) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($path)
    if ($name -notmatch '^[a-z0-9]+(_[a-z0-9]+)*$') {
        Add-Err "Not snake_case: $path"
    }
}

if (-not (Test-Path "include/remindme")) {
    Add-Err "Missing include/remindme directory."
}

$headerFiles = @()
if (Test-Path "include/remindme") {
    $headerFiles = Get-ChildItem "include/remindme" -File
    foreach ($f in $headerFiles) {
        if ($f.Extension -ne ".hpp") {
            Add-Err "Header extension must be .hpp: $($f.FullName)"
        }
        Assert-SnakeCaseFileName $f.Name

        $content = Get-Content -Raw $f.FullName
        if ($content -notmatch '(?m)^\s*namespace\s+remindme\b') {
            Add-Err "Header missing `namespace remindme`: $($f.FullName)"
        }
    }
}

$sourceFiles = @()
if (Test-Path "src") {
    $sourceFiles = Get-ChildItem "src" -File -Filter *.cpp
    foreach ($f in $sourceFiles) {
        $n = $f.Name
        if ($n -ne "main.cpp") {
            Assert-SnakeCaseFileName $n
        }
    }
}

$testFiles = @()
if (Test-Path "tests") {
    $testFiles = Get-ChildItem "tests" -File -Filter *.cpp
    foreach ($f in $testFiles) {
        Assert-SnakeCaseFileName $f.Name
    }
}

$projectCppFiles = @($sourceFiles + $testFiles)
foreach ($f in $projectCppFiles) {
    $content = Get-Content -Raw $f.FullName
    if ($content -match '#include\s+"[^"]+\.h"') {
        Add-Err "Use .hpp includes (found .h include): $($f.FullName)"
    }
}

if ($errors.Count -gt 0) {
    Write-Host "Convention check failed:`n" -ForegroundColor Red
    foreach ($e in $errors) {
        Write-Host "- $e" -ForegroundColor Red
    }
    exit 1
}

Write-Host "Convention check passed." -ForegroundColor Green
exit 0
