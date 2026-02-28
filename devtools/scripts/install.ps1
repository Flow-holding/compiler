# install.ps1 — Installa flow su Windows
# Uso: irm https://github.com/Flow-holding/compiler/releases/latest/download/install.ps1 | iex

$REPO    = "Flow-holding/compiler"
$BIN_DIR = "$env:LOCALAPPDATA\flow\bin"

Write-Host "Installazione flow..." -ForegroundColor Cyan

New-Item -ItemType Directory -Force -Path $BIN_DIR | Out-Null

$api     = "https://api.github.com/repos/$REPO/releases/latest"
$release = Invoke-RestMethod -Uri $api -Headers @{ "User-Agent" = "flow-installer" }

function Install-Binary($assetName, $destName) {
    $asset = $release.assets | Where-Object { $_.name -eq $assetName }
    if (-not $asset) {
        Write-Host "Errore: $assetName non trovato nella release" -ForegroundColor Red
        exit 1
    }
    $dest = "$BIN_DIR\$destName"
    Write-Host "Download $assetName..."
    Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $dest
    Unblock-File -Path $dest
}

Install-Binary "flow-windows-x64.exe"  "flow.exe"
Install-Binary "flowc-windows-x64.exe" "flowc.exe"

# Aggiunge al PATH utente
$userPath = [Environment]::GetEnvironmentVariable("PATH", "User")
if ($userPath -notlike "*$BIN_DIR*") {
    [Environment]::SetEnvironmentVariable("PATH", "$userPath;$BIN_DIR", "User")
    Write-Host "Aggiunto $BIN_DIR al PATH" -ForegroundColor Green
}

Write-Host ""
Write-Host "flow installato ($($release.tag_name))!" -ForegroundColor Green
Write-Host "  Riapri il terminale e scrivi: flow help"
