#!/bin/sh
# cli/build.sh — Build del CLI Flow (Linux/macOS)
# Con webview cross-platform (WebKit su macOS, WebKitGTK su Linux)
# Uso: ./build.sh            (debug)
#      ./build.sh --prod     (ottimizzato)
#      ./build.sh --out flow
# Linux: apt install libgtk-3-dev libwebkit2gtk-4.1-dev

PROD=0
OUT="flow"

for arg in "$@"; do
    case "$arg" in
        --prod) PROD=1 ;;
        --out=*) OUT="${arg#--out=}" ;;
    esac
done

SRC_C="main.c util.c config.c flowc.c cmd_init.c cmd_build.c cmd_dev.c cmd_update.c wv_webview.c"
WEBVIEW_SRC="deps/webview-tmp/core/src/webview.cc"

if [ "$PROD" = "1" ]; then
    FLAGS="-O2 -DNDEBUG"
else
    FLAGS="-g -O0"
fi

INC="-Ideps/webview-tmp/core/include"

# Platform-specific: webview deps
OS=$(uname -s)
case "$OS" in
    Darwin)
        WV_CFLAGS="$(pkg-config --cflags 2>/dev/null || true)"
        WV_LIBS="-framework WebKit -lpthread -ldl"
        ;;
    Linux)
        if pkg-config --exists gtk+-3.0 webkit2gtk-4.1 2>/dev/null; then
            WV_CFLAGS="$(pkg-config --cflags gtk+-3.0 webkit2gtk-4.1)"
            WV_LIBS="$(pkg-config --libs gtk+-3.0 webkit2gtk-4.1) -lpthread -ldl"
        elif pkg-config --exists gtk4 webkitgtk-6.0 2>/dev/null; then
            WV_CFLAGS="$(pkg-config --cflags gtk4 webkitgtk-6.0)"
            WV_LIBS="$(pkg-config --libs gtk4 webkitgtk-6.0) -lpthread -ldl"
        else
            echo "✗ WebKitGTK non trovato. Installa: libgtk-3-dev libwebkit2gtk-4.1-dev"
            exit 1
        fi
        ;;
    *)
        WV_CFLAGS=""
        WV_LIBS="-lpthread -ldl"
        ;;
esac

if [ ! -f "$WEBVIEW_SRC" ]; then
    echo "✗ Webview non trovato. Esegui:"
    echo "  git clone --depth 1 https://github.com/webview/webview.git deps/webview-tmp"
    exit 1
fi

echo "Build CLI Flow ($OS + webview)..."
# Compila C
clang $FLAGS $INC -c $SRC_C || exit 1
# Compila webview.cc
clang++ $FLAGS $INC $WV_CFLAGS -std=c++11 -c $WEBVIEW_SRC -o webview.o || exit 1
# Link
clang++ *.o -o "$OUT" $WV_LIBS || exit 1
rm -f *.o

if [ -f "$OUT" ]; then
    SIZE=$(du -k "$OUT" | cut -f1)
    echo "✓ $OUT (${SIZE}KB)"
else
    echo "✗ Build fallita"
    exit 1
fi
