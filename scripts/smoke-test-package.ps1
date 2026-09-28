# SPDX-License-Identifier: MIT

# Launches the packaged app with PATH reduced to Windows system directories,
# mirroring an end-user machine without MSYS2/Qt installed. Passes if the app
# is still running after the startup window; fails if it exits early (for
# example with STATUS_DLL_NOT_FOUND because a DLL was not bundled).

param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDir,
    [string]$ExeName = "RemindMe.exe",
    [int]$StartupSeconds = 10
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$exePath = Join-Path (Resolve-Path $PackageDir).Path $ExeName
if (-not (Test-Path $exePath)) {
    throw "Packaged executable not found: $exePath"
}

# Suppress the loader's "DLL not found" dialog (inherited by the child) so a
# broken package exits with an error code instead of blocking on a popup.
Add-Type -Namespace RemindMe -Name NativeMethods -MemberDefinition @'
[DllImport("kernel32.dll")]
public static extern uint SetErrorMode(uint uMode);
'@
$SEM_FAILCRITICALERRORS = 0x0001
$SEM_NOGPFAULTERRORBOX = 0x0002
[void][RemindMe.NativeMethods]::SetErrorMode($SEM_FAILCRITICALERRORS -bor $SEM_NOGPFAULTERRORBOX)

$knownExitCodes = @{
    "C0000135" = "STATUS_DLL_NOT_FOUND (a required DLL is missing from the package)"
    "C0000139" = "STATUS_ENTRYPOINT_NOT_FOUND (a bundled DLL has the wrong version)"
    "C000007B" = "STATUS_INVALID_IMAGE_FORMAT (a bundled DLL has the wrong architecture)"
    "C0000142" = "STATUS_DLL_INIT_FAILED"
}

$windowsDir = [Environment]::GetFolderPath([Environment+SpecialFolder]::Windows)
$systemDir = [Environment]::GetFolderPath([Environment+SpecialFolder]::System)
$originalPath = $env:PATH
$env:PATH = "$systemDir;$windowsDir;$(Join-Path $systemDir 'Wbem')"
$env:QT_FORCE_STDERR_LOGGING = "1"
try {
    $process = Start-Process -FilePath $exePath -WorkingDirectory (Split-Path -Parent $exePath) -PassThru
} finally {
    $env:PATH = $originalPath
}

if ($process.WaitForExit($StartupSeconds * 1000)) {
    $code = "{0:X8}" -f $process.ExitCode
    $reason = if ($knownExitCodes.ContainsKey($code)) { $knownExitCodes[$code] } else { "unexpected early exit" }
    throw ("{0} exited during startup with code 0x{1}: {2}" -f $ExeName, $code, $reason)
}

Stop-Process -Id $process.Id -Force
Write-Host ("Smoke test passed: {0} started from the package with a system-only PATH." -f $ExeName) -ForegroundColor Green
