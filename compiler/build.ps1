# build.ps1 — Compila il compiler Flow in C → flowc.exe
# Uso: .\build.ps1          (debug)
#      .\build.ps1 -prod    (ottimizzato)
param([switch]$prod)

$SRC   = @("main.c", "lexer.c", "parser.c", "codegen.c")
$OUT   = "flowc.exe"
$FLAGS = if ($prod) { "-O2" } else { "-g -O0" }

Write-Host "Build compiler C..." -ForegroundColor Cyan

$files = $SRC | ForEach-Object { "`"$_`"" }
$cmd   = "clang $flags $($files -join ' ') -o $OUT"

Write-Host "  $cmd"
Invoke-Expression $cmd

if ($LASTEXITCODE -eq 0) {
    $size = [math]::Round((Get-Item $OUT).Length / 1KB)
    Write-Host "✓ $OUT ($size KB)" -ForegroundColor Green
} else {
    Write-Host "✗ Build fallita" -ForegroundColor Red
    exit 1
}
