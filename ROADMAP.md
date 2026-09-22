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
| 11 | Modular sound sources | partial | Done (0.10.0): up to eight wavetable oscillators in one sound (A and B plus C to H, switched on with + OSC or Add Oscillator in the library, each with its own table, warp, unison, detune, blend, width, level, pan, pitch and filter switch; FM for C to H comes from A). The Sources widget is a mixer of every source (oscillators, sub, noise, sampler): on light, level, pan, filter routing, click a name to open its panel. Worst-case CPU with all eight at full unison measured at 8.6% of realtime (`SmokeTest --bench`). Missing: other source types as addable modules, per-source routing to buses (item 12). |
| 12 | Flexible audio routing | not started | |
| 13 | Rearrangeable effects racks | partial | Done (0.8.0, rebuilt in 0.10.0): the Effects Rack. Every effect is its own module with all its controls and a live display; drag a module by its name along the chain and the others spring out of the way; + ADD puts an effect in the rack, the x takes it out (and switches it off). Reordering while playing dips instead of clicking (tested). The order and rack are saved in sessions and presets and are undoable; chain presets (`.hnchain`) keep the order and every setting. The rack scrolls when squeezed: pull it by its background with momentum, and its ends rubber-band. Each unit has its own colour, its place in the chain, patch leads between units, a right-click menu (off, move earlier/later, reset, replace, remove) and a rack dry/wet on the output stage. Missing: multiple instances of one effect and moving effects between channels (needs item 12). |
| 14 | Proper effect editors | partial | Done (0.10.0): each rack module is a full editor with a live 3D display (spectrum waterfalls, the delay's echoes, the reverb's tail, the gate's pattern, the stereo field). The EQ is five bands (low cut, low shelf, two bells, high shelf) with draggable points on its curve; it matches the old shelves to within 0.013 dB (`SmokeTest --eqcheck`). Every effect control can be modulated: drag an LFO, envelope or macro chip onto it. Missing: searchable replacement. |
| 15 | Dedicated sampler | implemented (0.7.0) | A sampler widget alongside the oscillators. Import by dropping a file anywhere, or with LOAD (WAV, AIFF, FLAC, MP3, M4A, CAF, OGG; up to 60 s). The waveform editor has draggable start/end flags and loop brackets and a live playhead. The root note is detected on load (with fine tuning) and the start is set to the onset. Key tracking on or off, semitone and fine tuning, no loop, forward loop (crossfaded seam) or ping-pong, reverse, its own ADSR, level and pan, and a filter route. Replace and remove are in the LOAD menu. The sample is embedded as FLAC in sessions and exported presets, so there are no missing files. Tested with `SmokeTest --sampler`. Multi-sample zones belong to item 28 and slicing to item 21. |
| 16 | Customisable widget interface | partial | Done: the panels are widgets in a docking layout, so there are no gaps or overlaps. You can grab a widget by its title in normal use (or anywhere in layout mode) and drop it to stack as a tab, split beside another widget or dock along an edge. Gaps drag to resize. Given more room, a widget grows its displays instead of zooming. Widgets can be collapsed to a strip, maximised, hidden, replaced and duplicated, and structural changes have undo/redo. There's a widget library with click-to-add or drag-to-place, and new tool widgets (Scope, Loudness Meter, XY Pad, Mod Monitor, Macros, Pinboard). Pinned controls: right-click any knob and choose Pin to pinboard. 0.10.0: panels, drag ghosts and rack modules move on interruptible, velocity-aware springs (a dropped panel lands at the speed it was thrown; Reduce Motion is honoured); new widgets join a tab stack rather than squeezing panels below their minimum; layout-mode titles never run into their buttons (the lint fails if they do); switched-off sources fade back. Missing: the optional routing view (needs item 12). |
| 17 | Hypernova FX companion plugin | not started | |
| 18 | Saved workspaces | partial | Done: Sound Design, Sampling, Effects and Analysis defaults; save, load, rename, duplicate, delete and reset; the last workspace is restored and each keeps its own edits. Layouts, including tool settings, are stored in `workspaces.xml`, separate from patch state; switching never touches the sound (tested). The Sampling default is there (0.7.0). Missing: opt-in patch-associated layouts. |
| 19 | Reusable mini-instruments | not started | |
| 21 | Chop Lab | not started | Builds on the sampler (item 15). |
| 23 | Interchangeable synthesis engines | not started | |
| 24 | Physical models and resonators | not started | |
| 25 | Expanded modular modulation | partial | Existing: 2 LFOs, a mod envelope, drag-to-modulate, the mod matrix and macros. 0.10.0: 27 effect controls and every oscillator's position and level are destinations; drag a knob's modulation ring to set how much is sent, and right-click a knob for each source's depth. Next release: LFO 3 and LFO 4 as widgets from the library (new mod sources and rate destinations), and a Drawn LFO shape for all four LFOs (drag points, click to add, double-click to remove, shift snaps to a 16-step grid; saved with the sound, undoable, reset by presets; `SmokeTest --lfo`). Missing: drawable envelopes, envelope followers, modulation conditioning (curves, smoothing, quantise per slot). |
| 28 | Instrument zones and layers | not started | |
| 29 | Sound worlds | not started | |
| 30 | Patch exploration and learning | not started | |
| 31 | Resample into a new module | not started | |
| 32 | Character modules | partial | Existing: TAPE (wobble, noise, saturate) and distortion styles as fixed effects. Missing: placing them per layer or bus, early-digital sampling and companding, speaker/mic colouration. |
| — | Everyday workflow | implemented (0.10.0) | A/B compare (an undoable switch; presets load into the slot you're on; sessions remember the slot), Init in the new-sound menu, type-in values (double-click a knob's value), shift-drag for fine moves, pressed states that show on the press. Tested with `SmokeTest --compare`. |
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
- `UISnapshot <dir> --layouttest` also covers: adding up to eight oscillators and removing one (Sources rows follow), the rack (add, drag along the chain, remove, scroll, pull past the ends and spring back, flick), and the motion maths (critically damped springs, redirecting mid-flight, thrown springs, projection, rubber-banding).
- `UISnapshot <dir> --lint`: every workspace, in use and in layout mode: nothing poking out of its parent, no overlapping siblings, button text that fits, knob labels not squashed, nothing over the macro tray, layout-mode titles clear of their buttons.
- `UISnapshot <dir> --paintbench`: the cost of a full frame at 2x, of one panel face, and the heaviest components.
- `UISnapshot <dir>`: renders every view, including `ui_10_sources.png` (Sources with two added oscillators). Set `HYPERNOVA_THEME` to render a theme as it ships (this machine's look tweaks are ignored). Tests use a scratch workspace folder (`HYPERNOVA_WORKSPACE_FOLDER`), never the user's.
- `SmokeTest` modes: default (every preset renders, CPU), `--newfx`, `--sampler`, `--fxorder`, `--lowarch`, `--eqcheck`, `--compare`, `--bench`.
