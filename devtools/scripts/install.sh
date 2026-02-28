#!/bin/sh
# install.sh — Installa flow su macOS / Linux
# Uso: curl -fsSL https://github.com/Flow-holding/compiler/releases/latest/download/install.sh | sh

REPO="Flow-holding/compiler"
BIN_DIR="$HOME/.flow/bin"

mkdir -p "$BIN_DIR"
echo "Installazione flow..."

OS=$(uname -s | tr '[:upper:]' '[:lower:]')
ARCH=$(uname -m)

case "$OS" in
  darwin) PLATFORM="macos" ;;
  linux)  PLATFORM="linux"  ;;
  *)      echo "Piattaforma non supportata: $OS"; exit 1 ;;
esac

case "$ARCH" in
  arm64|aarch64) ARCH="arm64" ;;
  *)             ARCH="x64"   ;;
esac

API="https://api.github.com/repos/$REPO/releases/latest"
RELEASE=$(curl -fsSL -H "User-Agent: flow-installer" "$API")

get_url() {
    echo "$RELEASE" | grep "browser_download_url" | grep "\"$1\"" \
        | sed 's/.*"browser_download_url": "\(.*\)"/\1/'
}

# Scarica flow e flowc
for BIN in "flow" "flowc"; do
    ASSET="${BIN}-${PLATFORM}-${ARCH}"
    URL=$(get_url "$ASSET")
    if [ -z "$URL" ]; then echo "Errore: $ASSET non trovato"; exit 1; fi
    echo "Download $ASSET..."
    curl -fsSL -o "$BIN_DIR/$BIN" "$URL"
    chmod +x "$BIN_DIR/$BIN"
done

# Installa estensione editor (Cursor e VSCode)
echo "Installazione estensione editor..."
VSCODE_ZIP_URL=$(get_url "flow-vscode.zip")
if [ -n "$VSCODE_ZIP_URL" ]; then
    ZIP_TMP="/tmp/flow-vscode.zip"
    curl -fsSL -o "$ZIP_TMP" "$VSCODE_ZIP_URL"
    for EDITOR in ".cursor" ".vscode"; do
        EXT_DIR="$HOME/$EDITOR/extensions/flow-language"
        if [ -d "$HOME/$EDITOR" ]; then
            rm -rf "$EXT_DIR"
            mkdir -p "$EXT_DIR"
            unzip -q "$ZIP_TMP" -d "$EXT_DIR"
            echo "  OK $EXT_DIR"
        fi
    done
    rm -f "$ZIP_TMP"
fi

# Aggiunge al PATH
SHELL_RC="$HOME/.bashrc"
[ -f "$HOME/.zshrc" ] && SHELL_RC="$HOME/.zshrc"
if ! grep -q "flow/bin" "$SHELL_RC" 2>/dev/null; then
    echo 'export PATH="$HOME/.flow/bin:$PATH"' >> "$SHELL_RC"
    echo "Aggiunto $BIN_DIR al PATH in $SHELL_RC"
fi

echo ""
echo "flow installato!"
echo "  Ricarica la shell: source $SHELL_RC"
echo "  Riavvia Cursor/VSCode per la syntax highlighting"
