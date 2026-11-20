#!/usr/bin/env bash
# Collects the three plug-ins, the apps and the manual into dist/Tantri-<os>/<product>/
# usage: ci/package.sh <macOS|Windows> <build dir>
set -euo pipefail
OS="$1"; B="$2"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/dist/Tantri-$OS"
rm -rf "$OUT"; mkdir -p "$OUT"

art() {
    for d in "$B/${1}_artefacts/Release" "$B/${1}_artefacts"; do
        [ -d "$d/VST3" ] && { echo "$d"; return; }
    done
    echo "missing artefacts for $1" >&2; exit 1
}

product() {   # target "file name" folder
    local tgt="$1" file="$2" P="$OUT/$3" A
    A="$(art "$tgt")"
    mkdir -p "$P"
    cp -R "$A/VST3/$file.vst3" "$P/"
    case "$OS" in
        macOS)
            cp -R "$A/AU/$file.component" "$P/"
            [ -d "$A/VST/$file.vst" ] && cp -R "$A/VST/$file.vst" "$P/"
            [ -d "$A/Standalone/$file.app" ] && cp -R "$A/Standalone/$file.app" "$P/"
            for f in "$P"/*; do codesign --force --deep -s - "$f"; done ;;
        Windows)
            [ -f "$A/VST/$file.dll" ] && cp "$A/VST/$file.dll" "$P/"
            [ -f "$A/Standalone/$file.exe" ] && cp "$A/Standalone/$file.exe" "$P/" ;;
    esac
    return 0
}

product Tantri "Tantri" "Tantri"
product TantriAkasha "Tantri Akasha" "Tantri Akasha"
product TantriAkashaFX "Tantri Akasha FX" "Tantri Akasha"
cp "$ROOT/docs/manual/Tantri Manual.pdf" "$ROOT/README.md" "$OUT/"
cat > "$OUT/INSTALL.txt" <<TXT
Tantri, Tantri Akasha, Tantri Akasha FX

macOS
  VST3 (.vst3)        -> ~/Library/Audio/Plug-Ins/VST3
  Audio Unit (.component) -> ~/Library/Audio/Plug-Ins/Components
  VST2 (.vst)         -> ~/Library/Audio/Plug-Ins/VST
  Apps (.app)         -> /Applications
  The builds are not notarised: if macOS refuses to open them, run
  xattr -dr com.apple.quarantine <the file>

Windows
  VST3 (.vst3)        -> C:\Program Files\Common Files\VST3
  VST2 (.dll)         -> your VST2 folder
  Apps (.exe)         -> anywhere
TXT

cd "$ROOT/dist"
case "$OS" in
    macOS)   ditto -c -k --keepParent "Tantri-$OS" "Tantri-$OS.zip" ;;
    Windows) 7z a -tzip -r "Tantri-$OS.zip" "Tantri-$OS" > /dev/null ;;
esac
ls -la "$ROOT/dist"
