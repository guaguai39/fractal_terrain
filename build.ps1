# ============================================================================
#  build.ps1 - Configure + build script (Visual Studio 2026 / MSVC v145)
# ----------------------------------------------------------------------------
#  Usage:
#     powershell -ExecutionPolicy Bypass -File build.ps1              # Release
#     powershell -ExecutionPolicy Bypass -File build.ps1 -Config Debug
#     powershell -ExecutionPolicy Bypass -File build.ps1 -Clean        # wipe build/
#
#  Why this script exists:
#     1) The host environment exports BOTH "Path" and "PATH". .NET's
#        ProcessStartInfo.EnvironmentVariables is case-sensitive, so MSBuild
#        throws "An item with the same key has already been added" (MSB6001)
#        and cl.exe never launches. This script de-duplicates first.
#     2) Proxy vars break Gitee access; they are removed for this process.
#
#  NOTE: keep this file ASCII-only. Windows PowerShell 5.1 parses .ps1 using
#        the system ANSI codepage, so non-ASCII comments corrupt the parser.
# ============================================================================
param(
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",
    [switch]$Clean,
    [switch]$NoBuild
)

$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot
Set-Location $Root

# --------------------------------------------------------------- tool paths
$CMake  = "D:\Visual Studio 2026\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$ClExe  = "D:/Visual Studio 2026/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/cl.exe"
$GitCmd = "D:\XIAZAIXIAZAIXIAZAI2\Git\Git\cmd"

if (-not (Test-Path $CMake)) { throw "CMake not found: $CMake" }
if (-not (Test-Path ($ClExe -replace '/', '\'))) { throw "cl.exe not found: $ClExe" }

# ------------------------------------------------------- environment cleanup
$pathValue = $env:PATH
[System.Environment]::SetEnvironmentVariable("Path", $null, "Process")
[System.Environment]::SetEnvironmentVariable("PATH", $null, "Process")

foreach ($v in @("HTTP_PROXY", "http_proxy", "HTTPS_PROXY", "https_proxy", "ALL_PROXY", "all_proxy")) {
    [System.Environment]::SetEnvironmentVariable($v, $null, "Process")
}

$parts = @()
foreach ($p in ($pathValue -split ';')) {
    $t = $p.Trim()
    if ($t -and ($parts -notcontains $t)) { $parts += $t }
}
$parts = @($GitCmd) + ($parts | Where-Object { $_ -ne $GitCmd })
$env:PATH = ($parts -join ';')
$env:GIT_TERMINAL_PROMPT = "0"

$dups = [System.Environment]::GetEnvironmentVariables().Keys |
        Group-Object { $_.ToUpper() } | Where-Object { $_.Count -gt 1 }
Write-Host "[env] duplicate env keys: $($dups.Count)   PATH entries: $($parts.Count)" -ForegroundColor DarkGray

# ------------------------------------------------------------------- cleanup
if ($Clean -and (Test-Path "$Root\build")) {
    Write-Host "[clean] removing build/" -ForegroundColor Yellow
    Remove-Item -LiteralPath "$Root\build" -Recurse -Force
}

# ----------------------------------------------------------------- configure
Write-Host "[configure] Visual Studio 18 2026 / x64 / $Config" -ForegroundColor Cyan
# GLM 0.9.9.8 declares cmake_minimum_required(3.1); CMake 4.x refuses < 3.5.
# Pass the policy floor via a variable so PowerShell cannot split "3.5".
$policyMin = "3.5"
$cfgArgs = @(
    "-B", "build", "-S", ".",
    "-G", "Visual Studio 18 2026",
    "-A", "x64",
    "-DCMAKE_C_COMPILER=$ClExe",
    "-DCMAKE_CXX_COMPILER=$ClExe",
    "-DCMAKE_POLICY_VERSION_MINIMUM=$policyMin"
)
& $CMake @cfgArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed (exit $LASTEXITCODE)" }

# --------------------------------------------------------------------- build
if (-not $NoBuild) {
    Write-Host "[build] compiling $Config" -ForegroundColor Cyan
    & $CMake --build build --config $Config --parallel
    if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)" }

    $exe = Join-Path $Root "build\bin\fractal_terrain.exe"
    if (Test-Path $exe) {
        $size = [math]::Round((Get-Item $exe).Length / 1KB, 1)
        Write-Host "[OK] built: $exe  ($size KB)" -ForegroundColor Green
    } else {
        Write-Host "[warn] executable not found under build/" -ForegroundColor Yellow
    }
}
