Download **Hypernova-0.14.1.dmg**, open it and double-click **Install Hypernova**. It installs over any older version and keeps your saved sounds. Signed and notarized by Apple. Apple Silicon + Intel, macOS 10.13 and later.

## fixes
- **adding a widget no longer crashes.** clicking a card in the widget library could take the host down with it: adding the widget rebuilds the library, which deletes the card, and the code was still reading that card's name afterwards. fixed at the root — every layout call now takes its own copy, and a card hands its click on after the click has finished.
- the same fix covers hiding, replacing, moving, maximising and collapsing panels, switching workspaces and soloing a source.

## the widget library, on the main page
- **a new button in the header opens the widget library** without going into layout mode first, and it stays open when layout mode goes off. click a widget to add it; it drops into the arrangement where there's room.
