# install.ps1 — Installa l'estensione Flow in Cursor/VSCode
# Uso: .\install.ps1

$EXT_SRC  = "$PSScriptRoot\vscode"
$CURSOR   = "$env:USERPROFILE\.cursor\extensions\flow-language-0.2.5"
$VSCODE   = "$env:USERPROFILE\.vscode\extensions\flow-language-0.2.5"

function Install-To($dest) {
    if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
    Copy-Item -Recurse $EXT_SRC $dest
    Write-Host "  ✓ installata in $dest" -ForegroundColor Green
}

Write-Host "Installazione estensione Flow Language..." -ForegroundColor Cyan

if (Test-Path "$env:USERPROFILE\.cursor") { Install-To $CURSOR }
if (Test-Path "$env:USERPROFILE\.vscode") { Install-To $VSCODE }

Write-Host ""
Write-Host "Riavvia Cursor/VSCode — i file .flow avranno syntax highlighting" -ForegroundColor Yellow
