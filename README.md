# Hypernova

Wavetable space synth in the Arrow family (JUCE, VST3/AU/Standalone, macOS). Serum-style engine with 320 sounds:
808s, amapiano log drums (a faithful rebuild of the FL Studio DX10 log drum), subs, reeses, growls and wobbles,
house stabs, techno and Euro, dub sirens and dancehall, keys, plucks, pads, strings and brass, bells, voices, arps,
synth drums, cinematic, retro/chip, world, leads and big shimmering soundscapes.

Look: cosmic cinematic. The nebula, stars and black hole behind the panels are a GPU fragment shader
(`Source/UI/Cosmos.h`), so the animation costs the CPU almost nothing; static layers are cached and the 3D views
only redraw while sound plays. Settings (gear) has window size and Animation Full / Calm / Off.
Workflow: undo/redo (Cmd+Z), a searchable preset browser with favourites that auditions as you arrow through,
and a randomiser that can nudge or mutate the current sound.

Audio: wavetables are read with 4-point Hermite interpolation and crossfaded band-limits. The Quality setting
(Eco / High / Ultra) oversamples the voice engine 1x / 2x / 4x, but only for patches that can alias (warps, FM,
filter drive); plain wavetables stay at 1x. Distortion is 4x oversampled, the output has a transparent -0.3 dBFS
peak limiter, knobs/automation are smoothed, and an idle instance sleeps once its effect tails fade.
`SmokeTest --fidelity [0|1|2]` measures aliasing per quality.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release          # installs VST3/AU to ~/Library/Audio/Plug-Ins
cmake --build build --target SmokeTest && ./build/SmokeTest_artefacts/Release/SmokeTest dist/demos    # every preset: NaN/silence/CPU, timing, chords, arp, preset files, pack
./build/SmokeTest_artefacts/Release/SmokeTest --bench                                                   # CPU under load (8-note chords)
./build/UISnapshot_artefacts/Release/UISnapshot dist/shots --paintbench                                # UI draw cost per component
cmake --build build --target UISnapshot && ./build/UISnapshot_artefacts/Release/UISnapshot dist/shots # editor PNGs (all three deck tabs)
auval -v aumu ArBs Arrw
```

After changing factory presets, relevel them: `python3 scripts/level_presets.py` (rewrites `Source/PresetTrims.h`).
After changing the Founders Pack sounds: `./build/MakeFoundersPack_artefacts/Release/MakeFoundersPack "packaging/FOUNDERS PACK"`.

## Release

Bump `project(Hypernova VERSION ...)` in CMakeLists.txt, commit, then:

```bash
./scripts/release.sh
```

It builds a universal (Apple Silicon + Intel) DMG with the branded **Install Hypernova** app (SwiftUI, `installer/app`,
built by `scripts/build-installer-app.sh`; it runs the bundled notarized .pkg with one password prompt), the FOUNDERS PACK
and an "Everything else" folder (the .pkg and read me), in a designed Finder window. Artwork (DMG backdrop, app/volume/folder
icons, macOS Installer art) is drawn with the plug-in's own style by `MakeInstallerArt`:
`cmake --build build --target MakeInstallerArt && ./build/MakeInstallerArt_artefacts/Release/MakeInstallerArt .`
(then `sips -s dpiHeight 144 -s dpiWidth 144 packaging/resources/background.png`). It signs and notarizes the plug-ins, installer and disk image,
and publishes a GitHub release. The installer replaces any older Hypernova (and the old "Arrow Bass") and never
touches the user's saved sounds. The plug-in codes (`Arrw`/`ArBs`) and state tag stay fixed so old sessions keep loading.

## Engine

- `Source/DSP/Wavetables.h`: 15 tables x 32 frames, FFT band-limited mip levels. Includes "Rich Sine", the DX10 carrier shape.
- `Source/DSP/Synth.h`: voice. Two unison oscillators (7 voices, width), warps (Sync, Bend, Mirror, PWM, Crush, true FM), sub, noise,
  pitch drop, glide, Poly/Mono/Legato, 8 filter types (incl. Comb and Formant), 2 envelopes, 2 LFOs with fade-in, drift, 8-slot mod matrix.
- `Source/DSP/Effects.h`: distortion (oversampled), OTT, chorus, ping-pong delay with tone, 8-line FDN reverb with shimmer, EQ, width, mono bass.
- `Source/PluginProcessor.*`: parameters, note handling (host note-offs are processed before note-ons at the same sample), chords + strum,
  arpeggiator, macros/LFOs driving the effects, presets, user presets, import/export.
- `Source/Presets.h`: factory presets built from shared bases. `Source/PresetTrims.h`: generated loudness levels.
- `Source/UI/`: 3D wavetable views, Sound Space (3D spectrum / phase-space orbit), tabbed deck (Modulation / Effects / Play).

## Sharing sounds

Presets are `.hnpreset` files (XML). Save to My Sounds, export to share, import by the preset menu or by dragging files or
whole pack folders onto the window. User sounds live in `~/Library/Application Support/Arrow/Hypernova/Presets`;
installed packs in `/Library/Application Support/Arrow/Hypernova/Packs`.
