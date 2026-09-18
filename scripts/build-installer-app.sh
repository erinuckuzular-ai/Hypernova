#!/bin/bash
# Builds "Install Hypernova.app", the branded installer, around a (notarized) Hypernova .pkg.
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
  swiftc -O -target "$arch-apple-macos13.0" -parse-as-library "$SRC/main.swift" "$SRC/UI.swift" -o "$BUILD/installer-$arch"
done
lipo -create "$BUILD/installer-arm64" "$BUILD/installer-x86_64" -output "$APP/Contents/MacOS/HypernovaInstaller"

cp "$PKG" "$APP/Contents/Resources/Hypernova.pkg"
cp "$ROOT/packaging/art/AppIcon.icns" "$APP/Contents/Resources/AppIcon.icns"

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
  <key>LSMinimumSystemVersion</key><string>13.0</string>
  <key>LSApplicationCategoryType</key><string>public.app-category.music</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>NSPrincipalClass</key><string>NSApplication</string>
  <key>NSAppleEventsUsageDescription</key><string>Hypernova's installer asks macOS for your password to install the plug-ins.</string>
</dict></plist>
PLIST

if [ -n "${APP_SIGN_ID:-}" ]; then
  codesign --force --options runtime --timestamp --sign "$APP_SIGN_ID" "$APP"
else
  codesign --force --sign - "$APP"
fi
codesign -v "$APP"
echo "$APP"
