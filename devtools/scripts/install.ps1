# install.ps1 — Installa flow su Windows
# Uso: irm https://github.com/Flow-holding/compiler/releases/latest/download/install.ps1 | iex

$REPO    = "Flow-holding/compiler"
$BIN_DIR = "$env:LOCALAPPDATA\flow\bin"

Write-Host "Installazione flow..." -ForegroundColor Cyan

New-Item -ItemType Directory -Force -Path $BIN_DIR | Out-Null

$api     = "https://api.github.com/repos/$REPO/releases/latest"
$release = Invoke-RestMethod -Uri $api -Headers @{ "User-Agent" = "flow-installer" }

function Get-Asset($name) {
    $asset = $release.assets | Where-Object { $_.name -eq $name }
    if (-not $asset) { Write-Host "Errore: $name non trovato nella release" -ForegroundColor Red; exit 1 }
    return $asset.browser_download_url
}

# Scarica flow.exe e flowc.exe
Write-Host "Download flow.exe..."
Invoke-WebRequest -Uri (Get-Asset "flow-windows-x64.exe")  -OutFile "$BIN_DIR\flow.exe"
Invoke-WebRequest -Uri (Get-Asset "flowc-windows-x64.exe") -OutFile "$BIN_DIR\flowc.exe"
Unblock-File "$BIN_DIR\flow.exe"
Unblock-File "$BIN_DIR\flowc.exe"

# Installa estensione editor (Cursor e VSCode)
Write-Host "Installazione estensione editor..."
$zipUrl  = Get-Asset "flow-vscode.zip"
$zipTmp  = "$env:TEMP\flow-vscode.zip"
Invoke-WebRequest -Uri $zipUrl -OutFile $zipTmp

foreach ($editor in @(".cursor", ".vscode")) {
    $extDir = "$env:USERPROFILE\$editor\extensions\flow-language"
    if (Test-Path "$env:USERPROFILE\$editor") {
        if (Test-Path $extDir) { Remove-Item -Recurse -Force $extDir }
        Expand-Archive -Path $zipTmp -DestinationPath $extDir -Force
        Write-Host "  OK $extDir" -ForegroundColor Green
    }
}
Remove-Item $zipTmp

# Aggiunge al PATH utente
$userPath = [Environment]::GetEnvironmentVariable("PATH", "User")
if ($userPath -notlike "*$BIN_DIR*") {
    [Environment]::SetEnvironmentVariable("PATH", "$userPath;$BIN_DIR", "User")
    Write-Host "Aggiunto $BIN_DIR al PATH" -ForegroundColor Green
}

Write-Host ""
Write-Host "flow $($release.tag_name) installato!" -ForegroundColor Green
Write-Host "  Riapri il terminale e scrivi: flow help"
Write-Host "  Riavvia Cursor/VSCode per la syntax highlighting"
