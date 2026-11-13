$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ProjectRoot "build"

Write-Host "Configuring PrivaLayer (x64 / Release)..." -ForegroundColor Cyan
cmake -S $ProjectRoot -B $BuildDir -A x64
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

Write-Host "Building PrivaLayer..." -ForegroundColor Cyan
cmake --build $BuildDir --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

$Exe = Join-Path $BuildDir "Release\PrivaLayer.exe"
if (-not (Test-Path $Exe)) {
    $Exe = Join-Path $BuildDir "PrivaLayer.exe"
}
if (Test-Path $Exe) {
    Write-Host "Build complete: $Exe" -ForegroundColor Green
} else {
    Write-Warning "Build finished, but the EXE was not found at the usual output path. Check the build directory."
}
