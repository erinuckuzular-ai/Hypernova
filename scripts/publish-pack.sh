#!/bin/bash
# Publishes a sound pack to the website: zips a folder of .hnpreset files (and any .hnwt wavetables)
# and uploads it as its own GitHub release, which the site's Sound Packs section lists automatically.
#
#   ./scripts/publish-pack.sh "path/to/My Pack" "One line about the pack"
#
# Pack releases are tagged pack-<name> and never marked "latest", so the plug-in's updater and the
# site's main download button keep pointing at the newest Hypernova release.
set -euo pipefail
cd "$(dirname "$0")/.."

SRC="${1:?usage: publish-pack.sh <pack folder> [description]}"
DESC="${2:-}"
[[ -d "$SRC" ]] || { echo "not a folder: $SRC" >&2; exit 1; }

NAME="$(basename "$SRC")"
SLUG="$(echo "$NAME" | tr '[:upper:]' '[:lower:]' | sed -E 's/[^a-z0-9]+/-/g; s/^-|-$//g')"
TAG="pack-$SLUG"
COUNT="$(find "$SRC" -name '*.hnpreset' | wc -l | tr -d ' ')"
[[ "$COUNT" -gt 0 ]] || { echo "no .hnpreset files in $SRC" >&2; exit 1; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
ZIP="$WORK/$NAME.zip"
( cd "$(dirname "$SRC")" && zip -qr "$ZIP" "$NAME" -i '*.hnpreset' '*.hnwt' '*.txt' '*.md' )

NOTES="$WORK/notes.md"
{
  [[ -n "$DESC" ]] && echo "$DESC" && echo
  echo "**$COUNT sounds.** Download the zip and drag it onto Hypernova (or use the preset menu > Import sounds or a pack...)."
  echo "The sounds appear in the browser under **$NAME**."
} > "$NOTES"

if gh release view "$TAG" >/dev/null 2>&1; then
  # Updating an existing pack: replace the zip and the notes.
  gh release upload "$TAG" "$ZIP" --clobber
  gh release edit "$TAG" --notes-file "$NOTES" --latest=false
else
  gh release create "$TAG" "$ZIP" --title "$NAME (sound pack)" --notes-file "$NOTES" --latest=false
fi
echo "Published $NAME ($COUNT sounds) as $TAG"
