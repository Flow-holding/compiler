# install.ps1 — Installa flow su Windows
# Uso: irm https://github.com/flow-lang/flow/releases/latest/download/install.ps1 | iex

$REPO    = "flow-lang/flow"
$BIN_DIR = "$env:LOCALAPPDATA\flow\bin"
$EXE     = "$BIN_DIR\flow.exe"

Write-Host "Installazione flow..." -ForegroundColor Cyan

# Crea cartella
New-Item -ItemType Directory -Force -Path $BIN_DIR | Out-Null

# Scarica l'ultimo release
$api      = "https://api.github.com/repos/$REPO/releases/latest"
$release  = Invoke-RestMethod -Uri $api -Headers @{ "User-Agent" = "flow-installer" }
$asset    = $release.assets | Where-Object { $_.name -eq "flow-windows-x64.exe" }

if (-not $asset) {
    Write-Host "Errore: binario non trovato nella release" -ForegroundColor Red
    exit 1
}

Write-Host "Download $($release.tag_name)..."
Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $EXE

# Aggiunge al PATH utente (persistente)
$userPath = [Environment]::GetEnvironmentVariable("PATH", "User")
if ($userPath -notlike "*$BIN_DIR*") {
    [Environment]::SetEnvironmentVariable("PATH", "$userPath;$BIN_DIR", "User")
    Write-Host "Aggiunto $BIN_DIR al PATH" -ForegroundColor Green
}

Write-Host ""
Write-Host "flow installato correttamente!" -ForegroundColor Green
Write-Host "  Riapri il terminale e scrivi: flow help"
