#!/bin/bash
# Wezt Opto Comp - Mac build + install script
cd "$(dirname "$0")" || exit 1

echo "=== Wezt Opto Comp: Mac build ==="

# 1. Apple command-line tools (compiler)
if ! xcode-select -p >/dev/null 2>&1; then
  echo "Apple's command-line tools are missing. A window will open: click Install,"
  echo "wait until it finishes, then run this script again."
  xcode-select --install
  exit 1
fi

# 2. CMake (installed with Python's pip if missing, no admin needed)
if ! command -v cmake >/dev/null 2>&1; then
  echo "Installing CMake..."
  python3 -m pip install --user --upgrade cmake || { echo "Could not install CMake."; exit 1; }
  export PATH="$(python3 -m site --user-base)/bin:$PATH"
fi
command -v cmake >/dev/null 2>&1 || { echo "CMake not found."; exit 1; }
cmake --version | head -1

# 3. Build (works on Apple Silicon and Intel Macs)
echo "Building. This takes about 10 minutes the first time..."
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCOPY_PLUGIN_AFTER_BUILD=OFF \
  "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" || { echo "CONFIGURE FAILED"; exit 1; }
cmake --build build --config Release --parallel || { echo "BUILD FAILED"; exit 1; }

# 4. Install
PLUGIN=$(find build -name "*.vst3" -maxdepth 6 -prune | head -1)
[ -z "$PLUGIN" ] && { echo "Could not find the built .vst3"; exit 1; }
DEST="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$DEST"
rm -rf "$DEST/Wezt Opto Comp.vst3"
cp -R "$PLUGIN" "$DEST/"
xattr -cr "$DEST/Wezt Opto Comp.vst3"      # removes the "unidentified developer" block
codesign --force --deep -s - "$DEST/Wezt Opto Comp.vst3" 2>/dev/null  # ad-hoc sign (needed on Apple Silicon)

echo ""
echo "DONE. Installed to: $DEST/Wezt Opto Comp.vst3"
echo "Open your music program and rescan plugins."
