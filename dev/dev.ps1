# Flow dev — builda, aggiorna binari, avvia
$cliDir = Join-Path $PSScriptRoot "..\cli"
$compDir = Join-Path $PSScriptRoot "..\compiler"
Push-Location $cliDir; & .\build.ps1; Pop-Location
Push-Location $compDir; & .\build.ps1; Pop-Location
Copy-Item (Join-Path $cliDir "flow.exe") (Join-Path $PSScriptRoot "flow.exe") -Force
Copy-Item (Join-Path $cliDir "WebView2Loader.dll") (Join-Path $PSScriptRoot "WebView2Loader.dll") -Force -ErrorAction SilentlyContinue
Copy-Item (Join-Path $compDir "flowc.exe") (Join-Path $PSScriptRoot "flowc.exe") -Force -ErrorAction SilentlyContinue
Push-Location $PSScriptRoot; & .\flow.exe dev; Pop-Location
