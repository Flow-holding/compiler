# cli/build.ps1 — Build del CLI Flow in C (Windows)
# Uso: .\build.ps1           (debug)
#      .\build.ps1 -prod     (ottimizzato)
#      .\build.ps1 -out flow.exe
param(
    [switch]$prod,
    [string]$out = "flow.exe"
)

$SRC = @(
    "main.c", "util.c", "config.c", "flowc.c",
    "cmd_init.c", "cmd_build.c", "cmd_dev.c", "cmd_update.c"
)

$FLAGS  = if ($prod) { "-O2 -DNDEBUG" } else { "-g -O0" }
$LFLAGS = "-lws2_32"

Write-Host "Build CLI Flow (Windows)..." -ForegroundColor Cyan

$files = $SRC | ForEach-Object { "`"$_`"" }
$cmd   = "clang $FLAGS $($files -join ' ') -o `"$out`" $LFLAGS"
Write-Host "  $cmd"
Invoke-Expression $cmd

if ($LASTEXITCODE -eq 0) {
    $kb = [math]::Round((Get-Item $out).Length / 1KB)
    Write-Host "✓ $out ($kb KB)" -ForegroundColor Green
} else {
    Write-Host "✗ Build fallita" -ForegroundColor Red
    exit 1
}
