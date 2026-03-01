# Flow dev — builda, aggiorna binari, avvia
$cliDir = Join-Path $PSScriptRoot "..\cli"
$compDir = Join-Path $PSScriptRoot "..\compiler"

# Webview: clone se mancante
$webviewDir = Join-Path $cliDir "deps\webview-tmp"
if (-not (Test-Path (Join-Path $webviewDir "core\src\webview.cc"))) {
    Write-Host "Clone webview..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Force (Join-Path $cliDir "deps") | Out-Null
    Push-Location (Join-Path $cliDir "deps")
    git clone --depth 1 https://github.com/webview/webview.git webview-tmp
    Pop-Location
}

Push-Location $cliDir; & .\build.ps1; Pop-Location
Push-Location $compDir; & .\build.ps1; Pop-Location
Copy-Item (Join-Path $cliDir "flow.exe") (Join-Path $PSScriptRoot "flow.exe") -Force
Copy-Item (Join-Path $compDir "flowc.exe") (Join-Path $PSScriptRoot "flowc.exe") -Force -ErrorAction SilentlyContinue
Push-Location $PSScriptRoot; & .\flow.exe dev; Pop-Location
