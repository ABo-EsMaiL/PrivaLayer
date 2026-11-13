$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Candidates = @(
    (Join-Path $ProjectRoot "build\Release\PrivaLayer.exe"),
    (Join-Path $ProjectRoot "build\PrivaLayer.exe")
)
$Exe = $Candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Exe) {
    Write-Host "PrivaLayer.exe not found. Run .\build.ps1 first." -ForegroundColor Yellow
    exit 1
}
Start-Process -FilePath $Exe -WorkingDirectory (Split-Path -Parent $Exe)
