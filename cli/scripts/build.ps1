# cli/scripts/build.ps1 — Build del CLI Flow (Windows)
# Con webview cross-platform (WebView2 su Windows)
# Uso (dalla cartella cli/): .\scripts\build.ps1
#      .\scripts\build.ps1 -prod     (ottimizzato)
#      .\scripts\build.ps1 -out flow.exe
# Richiede: deps/webview-tmp (git clone webview), deps/webview2 (auto-download)

param(
    [switch]$prod,
    [string]$out = "flow.exe"
)

$SRC_C = @(
    "main.c", "util.c", "config.c", "flowc.c",
    "cmd\cmd_init.c", "cmd\cmd_build.c", "cmd\cmd_dev.c", "cmd\cmd_update.c",
    "webview\wv_webview.c"
)

$WEBVIEW_CC = "deps\webview-tmp\core\src\webview.cc"
$FLAGS  = if ($prod) { "-O2 -DNDEBUG" } else { "-g -O0" }
$LFLAGS = "-lws2_32 -ladvapi32 -lole32 -lshell32 -lshlwapi -luser32 -lversion"

# WebView2: scarica deps se mancanti
$WV2_DIR = "deps\webview2"
$WV2_INC = "$WV2_DIR\build\native\include"

if (-not (Test-Path $WV2_INC)) {
    Write-Host "Scarico WebView2 SDK..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Force deps | Out-Null
    Invoke-WebRequest -Uri "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.2210.55" -OutFile "deps\webview2.nupkg" -UseBasicParsing
    Copy-Item "deps\webview2.nupkg" "deps\webview2.zip"
    Expand-Archive -Path "deps\webview2.zip" -DestinationPath $WV2_DIR -Force
}

# Webview: verifica presenza
if (-not (Test-Path "deps\webview-tmp\core\src\webview.cc")) {
    Write-Host "Webview non trovato. Esegui:" -ForegroundColor Red
    Write-Host "  git clone --depth 1 https://github.com/webview/webview.git deps/webview-tmp" -ForegroundColor Yellow
    exit 1
}

$INC = "-I. -I`"deps\webview-tmp\core\include`" -I`"$WV2_INC`""
$CFLAGS = "$INC -D_WIN32_WINNT=0x0A00 -D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS"

Write-Host "Build CLI Flow (Windows + webview)..." -ForegroundColor Cyan

# Compila C
$cFiles = $SRC_C | ForEach-Object { "`"$_`"" }
$cmdC = "clang $FLAGS $CFLAGS $($cFiles -join ' ') -c"
Write-Host "  $cmdC"
Invoke-Expression $cmdC
if ($LASTEXITCODE -ne 0) { exit 1 }

# Compila webview.cc (C++)
$cmdCpp = "clang++ $FLAGS $CFLAGS -std=c++14 -DWEBVIEW_STATIC -c `"$WEBVIEW_CC`""
Write-Host "  $cmdCpp"
Invoke-Expression $cmdCpp
if ($LASTEXITCODE -ne 0) { exit 1 }

# Link
$objFiles = ($SRC_C | ForEach-Object { (Get-Item $_).BaseName + ".o" }) + @("webview.o")
$objQuoted = ($objFiles | ForEach-Object { "`"$_`"" }) -join " "
$cmdLink = "clang++ $objQuoted -o `"$out`" $LFLAGS"
Write-Host "  $cmdLink"
Invoke-Expression $cmdLink
if ($LASTEXITCODE -ne 0) { exit 1 }

# Cleanup .o
Get-ChildItem *.o -ErrorAction SilentlyContinue | Remove-Item

if ($LASTEXITCODE -eq 0) {
    $kb = [math]::Round((Get-Item $out).Length / 1KB)
    Write-Host "OK $out - $kb KB" -ForegroundColor Green
} else {
    Write-Host "Build fallita" -ForegroundColor Red
    exit 1
}
