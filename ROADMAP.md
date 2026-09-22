# Hypernova roadmap checklist

Status is one of **implemented**, **partial** or **not started**. The numbering follows the original roadmap, so the gaps are deliberate: removed items stay removed. Update this file whenever an item's status changes, and never mark an item implemented until it has been built, tested and shipped.

Last updated: 2026-09-22

## Stages

1. Widget-based interface, source modules, sampling, routing, effects racks, workspaces, sound-bank expansion.
2. Deeper instrument construction, modulation, layering, reusable instruments, the FX companion plugin.
3. Additional engines, resonators, transformation, resampling, discovery and the other advanced sound-design tools.

The widget interface comes first because every later feature (sampler, extra sources, racks) arrives as a widget.

## Retained items

| # | Item | Status | Notes |
|---|---|---|---|
| 1 | Orbit: sound morphing | not started | The Sound Space "ORBIT" view is a visualiser, not this feature. |
| 2 | Event Horizon: freeze and transform | not started | |
| 3 | Sound DNA: patch breeding | not started | Must stay separate from the dice button (the randomize strip is removed). |
| 6 | Matter: creative audio transformation | not started | Wavetable import exists, but it converts to a wavetable, which isn't what this item asks for. |
| 7 | Constellation: sound discovery | not started | The conventional searchable browser exists and must be kept. There's no similarity search yet. |
| 9 | Low-End Architect | partial | Done (0.9.0): the Low End widget. A phase-coherent Linkwitz-Riley split (40–300 Hz, draggable on a live split spectrum). The upper band goes through the effects rack while the sub stays clean: under full distortion the sub's error drops from 0.95 to 0.10 (tested). The sub has its own level, warmth and mono, and a tempo-synced duck (a sidechain-style pump without a sidechain). There's a band-balance readout and a phone-speaker check that's monitoring-only and never saved. It adds no latency (IIR crossover), and switching it on mid-note doesn't click. Missing: real kick sidechain input (the host has to support a sidechain on instruments). |
| 10 | Living patches: expressive response | partial | Existing: velocity routing and the DRAG VEL chip. Missing: per-note expression (MPE), pressure and note-position responses. |
| 11 | Modular sound sources | not started | |
| 12 | Flexible audio routing | not started | |
| 13 | Rearrangeable effects racks | partial | Done (0.8.0): the FX Chain widget. Drag effects into any order and the others make room. Reordering while playing dips instead of clicking (tested). Click an effect's light to bypass it, or its name to open its controls. The order is saved in sessions and presets and is undoable. Effect-chain presets save the order and every effect setting (`.hnchain`). Automation stays attached because parameter IDs don't change. Missing: multiple instances of one effect, duplicate/remove/replace, and moving effects between channels (needs item 12). |
| 14 | Proper effect editors | partial | There are per-effect controls with units and on/off switches. Missing: curve displays, modulation access from the effect itself, searchable replacement. |
| 15 | Dedicated sampler | implemented (0.7.0) | A sampler widget alongside the oscillators. Import by dropping a file anywhere, or with LOAD (WAV, AIFF, FLAC, MP3, M4A, CAF, OGG; up to 60 s). The waveform editor has draggable start/end flags and loop brackets and a live playhead. The root note is detected on load (with fine tuning) and the start is set to the onset. Key tracking on or off, semitone and fine tuning, no loop, forward loop (crossfaded seam) or ping-pong, reverse, its own ADSR, level and pan, and a filter route. Replace and remove are in the LOAD menu. The sample is embedded as FLAC in sessions and exported presets, so there are no missing files. Tested with `SmokeTest --sampler`. Multi-sample zones belong to item 28 and slicing to item 21. |
| 16 | Customisable widget interface | partial | Done: the panels are widgets in a docking layout, so there are no gaps or overlaps. You can grab a widget by its title in normal use (or anywhere in layout mode) and drop it to stack as a tab, split beside another widget or dock along an edge. Gaps drag to resize. Given more room, a widget grows its displays instead of zooming. Widgets can be collapsed to a strip, maximised, hidden, replaced and duplicated, and structural changes have undo/redo. There's a widget library with click-to-add or drag-to-place, and new tool widgets (Scope, Loudness Meter, XY Pad, Mod Monitor, Macros, Pinboard). Pinned controls: right-click any knob and choose Pin to pinboard. Missing: the optional routing view (needs item 12). |
| 17 | Hypernova FX companion plugin | not started | |
| 18 | Saved workspaces | partial | Done: Sound Design, Sampling, Effects and Analysis defaults; save, load, rename, duplicate, delete and reset; the last workspace is restored and each keeps its own edits. Layouts, including tool settings, are stored in `workspaces.xml`, separate from patch state; switching never touches the sound (tested). The Sampling default is there (0.7.0). Missing: opt-in patch-associated layouts. |
| 19 | Reusable mini-instruments | not started | |
| 21 | Chop Lab | not started | Builds on the sampler (item 15). |
| 23 | Interchangeable synthesis engines | not started | |
| 24 | Physical models and resonators | not started | |
| 25 | Expanded modular modulation | partial | Existing: 2 LFOs, a mod envelope, drag-to-modulate, the mod matrix and macros. Missing: addable LFOs, drawable envelopes, envelope followers, random sources, modulation conditioning. |
| 28 | Instrument zones and layers | not started | |
| 29 | Sound worlds | not started | |
| 30 | Patch exploration and learning | not started | |
| 31 | Resample into a new module | not started | |
| 32 | Character modules | partial | Existing: TAPE (wobble, noise, saturate) and distortion styles as fixed effects. Missing: placing them per layer or bus, early-digital sampling and companding, speaker/mic colouration. |
| — | New sound catalogue | partial | Donk & Bounce (0.9.0): 10 FM donks designed from the two-operator donk recipe and checked by measurement only (attack brightness 320–1,260 Hz, settled 160–430 Hz, in tune). The old "Donk Bass" is retired: hidden, but its program number is kept. 48 Club Studies presets were added, relevelled and measured (UK dub, UK garage, house bass and stabs, baile funk, club percussion, club experimental). They have been checked by measurement only, not by ear. More dusty keys, organs and hybrid instruments are still needed. |

## Interface removals (done)

- The randomize strip above FX is gone. The DSP and state behind it are kept.
- The dedicated Stereo tab is gone: Sound Space offers SPECTRUM and ORBIT. The stereo mode is still in the code for old sessions.
- The oversized main visualizer is gone. Sound Space is now a normal widget that can be hidden, resized or popped out.

## Explicitly excluded (do not implement)

- 4. Flight Recorder
- 5. Groove Gravity
- 8. Pilot Mode
- 20. Tape Desk
- 22. Dub Desk
- 26. Per-step parameter changes
- 27. Performance scenes

## Tests

- `UISnapshot <dir> --layouttest`: dock tree logic; the classic default layout; no gaps or overlaps after every operation; the overlay only takes titles and gaps in normal use; dragging gaps and titles through the real mouse handlers; maximise grows displays without ballooning knobs; hide, add, stack, split, edge dock, collapse, replace and duplicate; pinboard; undo/redo; workspace round trips; patch state unchanged.
- `UISnapshot <dir>`: renders `ui_6_layout_edit.png` and `ui_7_effects_workspace.png` along with the existing views. Set `HYPERNOVA_THEME` to render each theme. Tests use a scratch workspace folder, never the user's.
