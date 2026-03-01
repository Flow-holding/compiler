# cli/build.ps1 — Build del CLI Flow (Windows)
# Con WebView2 nativo per flow dev
# Uso: .\build.ps1           (debug)
#      .\build.ps1 -prod     (ottimizzato)
#      .\build.ps1 -out flow.exe

param(
    [switch]$prod,
    [string]$out = "flow.exe"
)

$SRC_C = @(
    "main.c", "util.c", "config.c", "flowc.c",
    "cmd_init.c", "cmd_build.c", "cmd_dev.c", "cmd_update.c"
)

$WV_CPP = "wv.cpp"
$FLAGS  = if ($prod) { "-O2 -DNDEBUG" } else { "-g -O0" }
$LFLAGS = "-lws2_32 -lole32 -luser32"

# WebView2: scarica deps se mancanti
$WV_DIR = "deps\webview2"
$WV_INC = "$WV_DIR\build\native\include"
$WV_LIB = "$WV_DIR\build\native\x64"
$WV_DLL = "$WV_DIR\build\native\x64\WebView2Loader.dll"

if (-not (Test-Path $WV_INC)) {
    Write-Host "Scarico WebView2 SDK..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Force deps | Out-Null
    Invoke-WebRequest -Uri "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.2210.55" -OutFile "deps\webview2.nupkg" -UseBasicParsing
    Copy-Item "deps\webview2.nupkg" "deps\webview2.zip"
    Expand-Archive -Path "deps\webview2.zip" -DestinationPath $WV_DIR -Force
}

$WV_CFLAGS = "-I`"$WV_INC`" -D_WIN32_WINNT=0x0A00 -D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS"
$WV_LINK   = "`"$WV_LIB\WebView2Loader.dll.lib`""

Write-Host "Build CLI Flow (Windows + WebView2)..." -ForegroundColor Cyan

# Compila C
$cFiles = $SRC_C | ForEach-Object { "`"$_`"" }
$cmdC = "clang $FLAGS $WV_CFLAGS $($cFiles -join ' ') -c"
Write-Host "  $cmdC"
Invoke-Expression $cmdC
if ($LASTEXITCODE -ne 0) { exit 1 }

# Compila wv.cpp (C++)
$cmdCpp = "clang++ $FLAGS $WV_CFLAGS -std=c++17 -c `"$WV_CPP`""
Write-Host "  $cmdCpp"
Invoke-Expression $cmdCpp
if ($LASTEXITCODE -ne 0) { exit 1 }

# Link
$objFiles = ($SRC_C + $WV_CPP) | ForEach-Object { (Get-Item $_).BaseName + ".o" } | ForEach-Object { "`"$_`"" }
$cmdLink = "clang++ $($objFiles -join ' ') -o `"$out`" $LFLAGS $WV_LINK"
Write-Host "  $cmdLink"
Invoke-Expression $cmdLink
if ($LASTEXITCODE -ne 0) { exit 1 }

# Copia WebView2Loader.dll accanto all'exe
$outDir = Split-Path $out -Parent
if ($outDir) { $dllDest = Join-Path $outDir "WebView2Loader.dll" } else { $dllDest = "WebView2Loader.dll" }
Copy-Item $WV_DLL $dllDest -Force

# Cleanup .o
Get-ChildItem *.o -ErrorAction SilentlyContinue | Remove-Item

if ($LASTEXITCODE -eq 0) {
    $kb = [math]::Round((Get-Item $out).Length / 1KB)
    Write-Host "OK $out - $kb KB" -ForegroundColor Green
} else {
    Write-Host "Build fallita" -ForegroundColor Red
    exit 1
}
