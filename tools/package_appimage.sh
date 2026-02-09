#!/usr/bin/env bash
set -euo pipefail

# Simple AppImage packaging helper for CFDojo (Linux)
# Usage: tools/package_appimage.sh [BUILD_DIR] [BINARY_NAME] [OUTPUT_APPIMAGE]
# Defaults: BUILD_DIR=build-cfdojo, BINARY_NAME=CFDojo, OUTPUT_APPIMAGE=CFDojo.AppImage

BUILD_DIR=${1:-build-cfdojo}
BINARY_NAME=${2:-CFDojo}
OUTPUT_APPIMAGE=${3:-CFDojo.AppImage}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APPDIR="$ROOT_DIR/AppDir"

echo "Using build dir: $BUILD_DIR"
echo "Binary: $BINARY_NAME"
echo "Output AppImage: $OUTPUT_APPIMAGE"

if [ ! -x "$BUILD_DIR/$BINARY_NAME" ]; then
  echo "Error: binary '$BUILD_DIR/$BINARY_NAME' not found or not executable. Build first." >&2
  exit 2
fi

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"

cp "$BUILD_DIR/$BINARY_NAME" "$APPDIR/usr/bin/"

# Minimal .desktop
mkdir -p "$APPDIR/usr/share/applications"
cat > "$APPDIR/usr/share/applications/cfdojo.desktop" <<EOF
[Desktop Entry]
Name=CFDojo
Exec=$BINARY_NAME
Icon=cfdojo
Type=Application
Categories=Development;IDE;
EOF

# Try to copy an icon if present in repo (common locations)
ICON_SRC=""
for p in "$ROOT_DIR/src/images/icon.png" "$ROOT_DIR/src/images/cfdojo.png" "$ROOT_DIR/resource.png"; do
  if [ -f "$p" ]; then
    ICON_SRC="$p"
    break
  fi
done

if [ -n "$ICON_SRC" ]; then
  mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
  cp "$ICON_SRC" "$APPDIR/usr/share/icons/hicolor/256x256/apps/cfdojo.png"
fi

# Locate linuxdeployqt
LINUXDEPLOYQT=${LINUXDEPLOYQT:-}
if [ -z "$LINUXDEPLOYQT" ]; then
  if command -v linuxdeployqt >/dev/null 2>&1; then
    LINUXDEPLOYQT=$(command -v linuxdeployqt)
  elif [ -x "$ROOT_DIR/tools/linuxdeployqt.AppImage" ]; then
    LINUXDEPLOYQT="$ROOT_DIR/tools/linuxdeployqt.AppImage"
  else
    echo "Error: linuxdeployqt not found. Install it or place linuxdeployqt.AppImage in tools/." >&2
    echo "See https://github.com/probonopd/linuxdeployqt/releases" >&2
    exit 3
  fi
fi

echo "Using linuxdeployqt: $LINUXDEPLOYQT"

pushd "$APPDIR" >/dev/null
"$LINUXDEPLOYQT" "usr/bin/$BINARY_NAME" -appimage
popd >/dev/null

if [ -f "$ROOT_DIR/$OUTPUT_APPIMAGE" ]; then
  echo "Created AppImage at: $ROOT_DIR/$OUTPUT_APPIMAGE"
else
  # linuxdeployqt usually places AppImage next to the executable name
  FOUND=$(find "$ROOT_DIR" -maxdepth 1 -type f -name "*${BINARY_NAME}*.AppImage" -print -quit || true)
  if [ -n "$FOUND" ]; then
    mv "$FOUND" "$ROOT_DIR/$OUTPUT_APPIMAGE"
    echo "Moved $FOUND -> $ROOT_DIR/$OUTPUT_APPIMAGE"
  else
    echo "AppImage not found after linuxdeployqt run. Check linuxdeployqt output." >&2
    exit 4
  fi
fi

echo "Done. Test on a clean Linux machine or container." 
