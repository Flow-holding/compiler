#!/bin/sh
# install.sh — Installa flow su macOS / Linux
# Uso: curl -fsSL https://github.com/flow-lang/flow/releases/latest/download/install.sh | sh

REPO="Flow-holding/compiler"
BIN_DIR="$HOME/.flow/bin"
EXE="$BIN_DIR/flow"

mkdir -p "$BIN_DIR"

echo "Installazione flow..."

# Rileva piattaforma
OS=$(uname -s | tr '[:upper:]' '[:lower:]')
ARCH=$(uname -m)

case "$OS" in
  darwin) PLATFORM="macos" ;;
  linux)  PLATFORM="linux" ;;
  *)      echo "Piattaforma non supportata: $OS"; exit 1 ;;
esac

case "$ARCH" in
  arm64|aarch64) ARCH="arm64" ;;
  *)             ARCH="x64" ;;
esac

ASSET="flow-${PLATFORM}-${ARCH}"

# Scarica dall'ultimo release
API="https://api.github.com/repos/$REPO/releases/latest"
URL=$(curl -fsSL -H "User-Agent: flow-installer" "$API" \
  | grep "browser_download_url" \
  | grep "$ASSET\"" \
  | sed 's/.*"browser_download_url": "\(.*\)"/\1/')

if [ -z "$URL" ]; then
  echo "Errore: binario non trovato per $PLATFORM/$ARCH"
  exit 1
fi

echo "Download $ASSET..."
curl -fsSL -o "$EXE" "$URL"
chmod +x "$EXE"

# Aggiunge al PATH nel profilo shell
SHELL_RC="$HOME/.bashrc"
[ -f "$HOME/.zshrc" ] && SHELL_RC="$HOME/.zshrc"

if ! grep -q "flow/bin" "$SHELL_RC" 2>/dev/null; then
  echo 'export PATH="$HOME/.flow/bin:$PATH"' >> "$SHELL_RC"
  echo "Aggiunto $BIN_DIR al PATH in $SHELL_RC"
fi

echo ""
echo "flow installato correttamente!"
echo "  Ricarica la shell:   source $SHELL_RC"
echo "  Poi scrivi:          flow help"
