#!/bin/bash
# Builds a universal (Apple Silicon + Intel) release and packages it as dist/Hypernova-<version>.dmg,
# containing a double-click installer (Install Hypernova.pkg).
#
# SKIP_BUILD=1 ARTEFACTS=build/Hypernova_artefacts/Release ./scripts/make-dmg.sh
#   packages an existing build instead (handy for testing the installer quickly).
#
# Optional environment variables (see README "Signing and notarizing the release"):
#   APP_SIGN_ID         "Developer ID Application: Name (TEAMID)" — signs the plug-ins and the DMG
#   INSTALLER_SIGN_ID   "Developer ID Installer: Name (TEAMID)"   — signs the .pkg
#   NOTARY_PROFILE      keychain profile from `xcrun notarytool store-credentials`
# Without them the build is ad-hoc signed: it works, but Gatekeeper makes users right-click > Open.
set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$PWD"
VERSION="$(sed -n 's/^project(Hypernova VERSION \([0-9.]*\)).*/\1/p' CMakeLists.txt)"
BUILD="$ROOT/build-release"
ARTEFACTS="${ARTEFACTS:-$BUILD/Hypernova_artefacts/Release}"
WORK="$BUILD/package"
DMG="$ROOT/dist/Hypernova-$VERSION.dmg"

if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
    echo "==> Building Hypernova $VERSION (universal)"
    cmake -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DHYPERNOVA_COPY_PLUGINS=OFF
    cmake --build "$BUILD" --config Release --target Hypernova_VST3 Hypernova_AU Hypernova_Standalone -j"$(sysctl -n hw.ncpu)"
fi

rm -rf "$WORK" && mkdir -p "$WORK"

if [[ -n "${APP_SIGN_ID:-}" ]]; then
    echo "==> Signing with Developer ID"
    for bundle in "$ARTEFACTS/VST3/Hypernova.vst3" "$ARTEFACTS/AU/Hypernova.component" "$ARTEFACTS/Standalone/Hypernova.app"; do
        # Hardened runtime + a secure timestamp: both are required for notarization.
        codesign --force --options runtime --timestamp --sign "$APP_SIGN_ID" "$bundle"
    done
else
    echo "==> Signing (ad-hoc — set APP_SIGN_ID for a Developer ID build)"
    for bundle in "$ARTEFACTS/VST3/Hypernova.vst3" "$ARTEFACTS/AU/Hypernova.component" "$ARTEFACTS/Standalone/Hypernova.app"; do
        codesign --force --sign - "$bundle"
    done
fi

echo "==> Building installer packages"
# One component package per part: <name> <payload> <install location> <legacy item to remove first>
# Each component's preinstall deletes the copy it is about to replace (and the old "Arrow Bass" one),
# so a newer DMG always installs cleanly over whatever is there.
make_component () {
    local name="$1" payload="$2" location="$3" legacy="$4"
    local root="$WORK/roots/$name" scripts="$WORK/scripts/$name"
    mkdir -p "$root" "$scripts"
    cp -R "$payload" "$root/"
    local item; item="$(basename "$payload")"

    cat > "$scripts/preinstall" <<SCRIPT
#!/bin/bash
rm -rf "$location/$item"
[ -n "$legacy" ] && rm -rf "$location/$legacy"
exit 0
SCRIPT
    if [[ "$name" == "au" ]]; then
        cp "$ROOT/packaging/scripts/postinstall" "$scripts/postinstall"
    fi
    chmod +x "$scripts/"*

    local plist=()
    if [[ "$name" != "pack" ]]; then
        pkgbuild --analyze --root "$root" "$WORK/$name.plist" >/dev/null
        plutil -replace 0.BundleIsRelocatable -bool NO "$WORK/$name.plist"
        plutil -replace 0.BundleOverwriteAction -string upgrade "$WORK/$name.plist"
        plist=(--component-plist "$WORK/$name.plist")
    fi

    pkgbuild --root "$root" \
             ${plist[@]+"${plist[@]}"} \
             --install-location "$location" \
             --identifier "com.arrow.hypernova.$name" \
             --version "$VERSION" \
             --scripts "$scripts" \
             "$WORK/pkgs/Hypernova-$name.pkg" >/dev/null
}
mkdir -p "$WORK/pkgs"
make_component vst3 "$ARTEFACTS/VST3/Hypernova.vst3"      "/Library/Audio/Plug-Ins/VST3"       "Arrow Bass.vst3"
make_component au   "$ARTEFACTS/AU/Hypernova.component"   "/Library/Audio/Plug-Ins/Components" "Arrow Bass.component"
make_component app  "$ARTEFACTS/Standalone/Hypernova.app" "/Applications"                      "Arrow Bass.app"
make_component pack "$ROOT/packaging/FOUNDERS PACK"       "/Library/Application Support/Arrow/Hypernova/Packs" ""

sed "s/@VERSION@/$VERSION/g" "$ROOT/packaging/distribution.xml" > "$WORK/distribution.xml"

STAGE="$WORK/dmg"
mkdir -p "$STAGE"
PKG="$STAGE/Install Hypernova.pkg"
PKG_SIGN_ARGS=()
[[ -n "${INSTALLER_SIGN_ID:-}" ]] && PKG_SIGN_ARGS=(--sign "$INSTALLER_SIGN_ID")
productbuild --distribution "$WORK/distribution.xml" \
             --resources "$ROOT/packaging/resources" \
             --package-path "$WORK/pkgs" \
             ${PKG_SIGN_ARGS[@]+"${PKG_SIGN_ARGS[@]}"} \
             "$PKG" >/dev/null
cp "$ROOT/packaging/READ ME FIRST.txt" "$STAGE/"
cp -R "$ROOT/packaging/FOUNDERS PACK" "$STAGE/FOUNDERS PACK"

# Notarizing the .pkg before it goes in the DMG means it also opens on its own.
if [[ -n "${NOTARY_PROFILE:-}" && -n "${INSTALLER_SIGN_ID:-}" ]]; then
    echo "==> Notarizing installer (Apple takes a few minutes)"
    xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$PKG"
fi

echo "==> Creating DMG"
mkdir -p "$ROOT/dist"
rm -f "$DMG"
hdiutil create -volname "Hypernova $VERSION" -srcfolder "$STAGE" -ov -format UDZO "$DMG" >/dev/null

if [[ -n "${APP_SIGN_ID:-}" ]]; then
    codesign --force --timestamp --sign "$APP_SIGN_ID" "$DMG"
    if [[ -n "${NOTARY_PROFILE:-}" ]]; then
        echo "==> Notarizing disk image"
        xcrun notarytool submit "$DMG" --keychain-profile "$NOTARY_PROFILE" --wait
        # Stapling the ticket means the DMG validates with no network.
        xcrun stapler staple "$DMG"
        spctl -a -vvv -t install "$DMG" || true
    fi
fi

echo "==> Done: $DMG"
