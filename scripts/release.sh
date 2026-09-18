#!/bin/bash
# One-command release: builds the signed + notarized universal DMG and publishes it as a GitHub release.
#   ./scripts/release.sh            (uses the version in CMakeLists.txt; bump it first)
# Needs: Developer ID certificates in the keychain, the "arrow-switch-notary" notarytool profile, and `gh` logged in.
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="$(sed -n 's/^project(Hypernova VERSION \([0-9.]*\)).*/\1/p' CMakeLists.txt)"
DMG="dist/Hypernova-$VERSION.dmg"
ROOT_NOTES="packaging/release-notes.md"   # edit before each release

APP_SIGN_ID="${APP_SIGN_ID:-Developer ID Application: ERIN UCKUZULAR (C3NF86XPV5)}" \
INSTALLER_SIGN_ID="${INSTALLER_SIGN_ID:-Developer ID Installer: ERIN UCKUZULAR (C3NF86XPV5)}" \
NOTARY_PROFILE="${NOTARY_PROFILE:-arrow-switch-notary}" \
./scripts/make-dmg.sh

spctl -a -t install "$DMG"
git push origin HEAD
gh release create "v$VERSION" "$DMG#Hypernova-$VERSION.dmg" --title "Hypernova $VERSION" \
    --notes-file "$ROOT_NOTES"
