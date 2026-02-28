#!/bin/sh
# cli/build.sh — Build del CLI Flow in C (Linux/macOS)
# Uso: ./build.sh            (debug)
#      ./build.sh --prod     (ottimizzato)
#      ./build.sh --out flow

PROD=0
OUT="flow"

for arg in "$@"; do
    case "$arg" in
        --prod) PROD=1 ;;
        --out=*) OUT="${arg#--out=}" ;;
    esac
done

SRC="main.c util.c config.c flowc.c cmd_init.c cmd_build.c cmd_dev.c cmd_update.c"

if [ "$PROD" = "1" ]; then
    FLAGS="-O2 -DNDEBUG"
else
    FLAGS="-g -O0"
fi

# Linker flags
OS=$(uname -s)
case "$OS" in
    Linux)  LFLAGS="-lpthread" ;;
    Darwin) LFLAGS=""           ;;
    *)      LFLAGS="-lpthread" ;;
esac

echo "Build CLI Flow ($OS)..."
CMD="clang $FLAGS $SRC -o $OUT $LFLAGS"
echo "  $CMD"
eval "$CMD"

if [ $? -eq 0 ]; then
    SIZE=$(du -k "$OUT" | cut -f1)
    echo "✓ $OUT (${SIZE}KB)"
else
    echo "✗ Build fallita"
    exit 1
fi
