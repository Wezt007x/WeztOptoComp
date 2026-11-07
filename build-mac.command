#!/bin/bash
# Wezt Plugins - Mac build + install script
cd "$(dirname "$0")" || exit 1
echo "=== Wezt Plugins: Mac build ==="
if ! xcode-select -p >/dev/null 2>&1; then
  echo "Apple's command-line tools are missing. A window will open: click Install,"
  echo "wait until it finishes, then run this script again."
  xcode-select --install
  exit 1
fi
if ! command -v cmake >/dev/null 2>&1; then
  echo "Installing CMake..."
  python3 -m pip install --user --upgrade cmake || { echo "Could not install CMake."; exit 1; }
  export PATH="$(python3 -m site --user-base)/bin:$PATH"
fi
command -v cmake >/dev/null 2>&1 || { echo "CMake not found."; exit 1; }
echo "Building all plugins. This takes about 15 minutes the first time..."
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCOPY_PLUGIN_AFTER_BUILD=OFF \
  "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" || { echo "CONFIGURE FAILED"; exit 1; }
cmake --build build --config Release --parallel || { echo "BUILD FAILED"; exit 1; }
DEST="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$DEST"
COUNT=0
while IFS= read -r PLUGIN; do
  NAME=$(basename "$PLUGIN")
  rm -rf "$DEST/$NAME"
  cp -R "$PLUGIN" "$DEST/"
  xattr -cr "$DEST/$NAME"
  codesign --force --deep -s - "$DEST/$NAME" 2>/dev/null
  echo "Installed: $NAME"
  COUNT=$((COUNT+1))
done < <(find build -name "*.vst3" -maxdepth 6 -prune)
echo ""
echo "DONE. Installed $COUNT plugins to $DEST"
echo "Open your music program and rescan plugins."
