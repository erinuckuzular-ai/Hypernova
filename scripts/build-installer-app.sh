#!/bin/bash
# Builds "Install Hypernova.app" (the branded installer, around a notarized Hypernova .pkg) and
# "Uninstall Hypernova.app" (the same app in uninstall mode).
# Usage: build-installer-app.sh <pkg> <out-dir> <version>
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PKG="$1"; OUT="$2"; VERSION="$3"
NAME="Install Hypernova"
APP="$OUT/$NAME.app"
SRC="$ROOT/installer/app"

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

BUILD="$(mktemp -d)"; trap 'rm -rf "$BUILD"' EXIT
for arch in arm64 x86_64; do
  swiftc -O -target "$arch-apple-macos12.0" -parse-as-library "$SRC/main.swift" "$SRC/UI.swift" -o "$BUILD/installer-$arch"
done
lipo -create "$BUILD/installer-arm64" "$BUILD/installer-x86_64" -output "$APP/Contents/MacOS/HypernovaInstaller"

cp "$PKG" "$APP/Contents/Resources/Hypernova.pkg"
cp "$ROOT/packaging/art/AppIcon.icns" "$APP/Contents/Resources/AppIcon.icns"
# Brand type for the installer window (SIL Open Font License).
cp "$ROOT/Resources/Fonts/SpaceGrotesk-var.ttf" "$ROOT/Resources/Fonts/SpaceMono-Regular.ttf" "$ROOT/Resources/Fonts/SpaceMono-Bold.ttf" "$APP/Contents/Resources/"

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleName</key><string>$NAME</string>
  <key>CFBundleDisplayName</key><string>$NAME</string>
  <key>CFBundleExecutable</key><string>HypernovaInstaller</string>
  <key>CFBundleIdentifier</key><string>com.arrow.hypernova.installer</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundleVersion</key><string>$VERSION</string>
  <key>CFBundleIconFile</key><string>AppIcon</string>
  <key>LSMinimumSystemVersion</key><string>12.0</string>
  <key>LSApplicationCategoryType</key><string>public.app-category.music</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>NSPrincipalClass</key><string>NSApplication</string>
  <key>NSAppleEventsUsageDescription</key><string>Hypernova's installer asks macOS for your password to install the plug-ins.</string>
</dict></plist>
PLIST

# The uninstaller is the same app opening straight onto its uninstall screen (no package inside).
UNAPP="$OUT/Uninstall Hypernova.app"
rm -rf "$UNAPP"
cp -R "$APP" "$UNAPP"
rm -f "$UNAPP/Contents/Resources/Hypernova.pkg"
plutil -replace CFBundleName -string "Uninstall Hypernova" "$UNAPP/Contents/Info.plist"
plutil -replace CFBundleDisplayName -string "Uninstall Hypernova" "$UNAPP/Contents/Info.plist"
plutil -replace CFBundleIdentifier -string "com.arrow.hypernova.uninstaller" "$UNAPP/Contents/Info.plist"
plutil -insert HNMode -string "uninstall" "$UNAPP/Contents/Info.plist"

for bundle in "$APP" "$UNAPP"; do
  if [ -n "${APP_SIGN_ID:-}" ]; then
    codesign --force --options runtime --timestamp --sign "$APP_SIGN_ID" "$bundle"
  else
    codesign --force --sign - "$bundle"
  fi
  codesign -v "$bundle"
done
echo "$APP"
