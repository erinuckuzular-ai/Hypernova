# Hypernova — handoff

This is everything another engineer (human or AI) needs to pick up Hypernova where it stands on 21 September 2026: what it is, how it got here, how the code is organised, how the work has been done so far, what is still open, and exactly what to do to ship an update — including the download page.

Owner: Erin (GitHub `erinuckuzular-ai`). The repo is public: https://github.com/erinuckuzular-ai/Hypernova
Download page: https://erinuckuzular-ai.github.io/Hypernova/
Current release: **0.5.1** (signed and notarized, published on GitHub).

---

## 1. What Hypernova is

A wavetable synthesizer plug-in for macOS (VST3, AU and a standalone app), built with JUCE 8 and CMake. It started as a bass synth ("Arrow Bass") aimed at 808s, Mike Dean / Travis Scott style sound design and amapiano log drums, and grew into a full synth.

Current feature set:

- **Sound:** 2 wavetable oscillators (15 built-in tables × 32 frames, position morph, 3D view), 7-voice unison, warps (sync, bend, mirror, PWM, crush, true FM), user wavetable import from WAV, audio-rate cross modulation (FM both ways, ring, AM, filter FM), sub oscillator, 8 noise types (white, pink, brown, blue, vinyl crackle, tape hiss, digital, wind), 8 filter types (LP12, LP24, HP, BP, notch, dirty, comb, formant), pitch drop envelope, glide, poly/mono/legato, drift, chord memory, strum, arpeggiator.
- **Modulation:** 2 envelopes with draggable handles, 2 LFOs (tempo sync, fade-in), an 8-slot mod matrix, 4 macros. Drag an LFO, the mod envelope or velocity onto any knob to modulate it; knobs draw a live modulation ring. Right-click any knob for the full source list, clear, or reset.
- **Effects:** distortion (6 types, 4× oversampled), tape wear, OTT, pitch shifter, chorus (classic/ensemble/dimension), flanger, FX filter with synced sweep, trance gate with patterns, auto-pan, delay (digital/reverse/granular, ping-pong), reverb (space/plate/spring/room, shimmer), EQ, width, mono bass. Click an effect's name to bypass it. Effects are split over two deck tabs, EFFECTS and MORE FX.
- **Sounds:** 362 factory presets (808s, log drums, subs, reeses, growls, house stabs, techno, dub/dancehall, keys, plucks, pads, strings, bells, voices, arps, synth drums, cinematic, retro, folk instruments, soundscapes, a 40-sound Experimental bank) plus the 22-sound **FOUNDERS PACK**. Presets are shareable `.hnpreset` files; `.zip` sound packs can be dragged onto the window.
- **Visuals:** Sound Space visualiser (3D spectrum, orbit, and a stereo view with goniometer, phase correlation and width by frequency), which can expand to fill the window or pop out into its own resizable window.
- **Looks:** five themes — **Paper** (the default house look), Cosmic, Blackout, Daylight, Vapor — plus user accent colour, backdrop scene and brightness, knob style (planet, ring, minimal, machined), panel style (glass, flat, outlined), keyboard toggle and "always show knob values". Settings are per user, saved in `~/Library/Application Support/Arrow/Hypernova/look.xml`.
- **Workflow:** preset browser (search, categories, favourites, audition while arrowing), undo/redo, dice menu (nudge/mutate/new) plus per-section dice with padlocks (oscillators, filter, modulation, effects), in-app update banner (checks GitHub at most once a day, one click downloads and opens the new installer).
- **Performance:** the voice engine oversamples only patches that alias; the plug-in sleeps when silent (~0% CPU); an open editor with nothing playing uses about 2% of a core (3.5% in the animated Cosmic theme). Audio benchmark: 1.5% average, 9.4% worst case per instance.
- **Compatibility:** universal binary; Intel builds run on macOS 10.13+, Apple Silicon on 11+. The installer app needs macOS 12+; older systems use the plain `.pkg` in the DMG's "Everything else" folder.

---

## 2. How we got here (chronological)

1. **0.1–0.2 (Arrow Bass → Hypernova).** First synth, 3D UI, 808s and Travis-style presets, DMG. The user reported notes not firing on time / muting (fixed: MIDI note-offs sorted before note-ons at the same sample; legato re-hit logic) and that the log drums weren't real log drums (rebuilt as an emulation of FL Studio's DX10 "Log Drum" patch, parameter for parameter). Renamed to Hypernova, 200+ sounds, house stabs, techno, dub sirens, dancehall, lots of customisation.
2. **Installer and sharing.** Branded installer app inside the DMG (SwiftUI) with uninstaller; installs overwrite older versions; FOUNDERS PACK; import/export/drag-drop of `.hnpreset`; repo made public; notarized releases.
3. **0.3.x.** "Cosmic cinematic" redesign with a GPU shader backdrop; researched competitor feature requests; 320 sounds; big CPU work (cached painting, adaptive oversampling, sleep when silent); higher fidelity (better interpolation, band-limiting, quality setting Eco/High/Ultra, transparent limiter, parameter smoothing).
4. **0.4.0.** Click-free note changes (voice handover fades, attack floor, look-ahead limiter); 808s rebuilt as real 808s (sine body, pitch knock, natural decay); "Afro"-tagged presets renamed (the user found them uncomfortable) and "World" became "Folk Instruments"; heavier subs on light basses; 40 Experimental presets from research into what producers ask for; smoother preset browser; in-app updates.
5. **0.4.1.** The user's Monterey (12.7.1) Mac couldn't open the AU or the installer. Cause: `CMAKE_OSX_DEPLOYMENT_TARGET` was set after `project()`, so builds silently targeted macOS 26. Fixed (10.13 Intel / 11 Apple Silicon), installer retargeted to macOS 12, plug-in open time cut from ~0.9 s to ~0.12 s. Notarization credentials had to be re-saved using the App Store Connect API key (see section 6).
6. **0.5.0.** Feedback from a friend (Kauai) plus research into Pigments/Vital/Serum requests: noise types, draggable envelopes, drag-to-modulate, click-name bypass, clearer mono/glide/legato, logo easter egg, stereo view and pop-out visualiser, wavetable import, cross-mod, the whole second effects rack, themes and customisation, bundled fonts, roomier layout, section dice, sound packs. Download site moved from a Cloudflare draft to **GitHub Pages from `docs/`** (the user asked for the same setup as their Arrow Switch project). Site and plug-in rebranded to a quieter, Teenage-Engineering-inspired voice (the user's stated competition): light, lowercase, spec-sheet copy, one orange accent. Paper theme became the default; the logo is an orange dot plus lowercase "hypernova".
7. **0.5.1.** The user reported "it's so bugged" and "heavy on the CPU". Driving the standalone app found: the PLAY page was empty (page switcher looped over 3 of 4 pages), pop-up menus ignored the plug-in's look, the preset browser was unreadable in light themes, right-click on knobs did nothing, theme switches left stale cached colours, the expanded visualiser was see-through, several clipped labels. CPU: idle editor 11–18% → ~2%. Plus a 3D pass (machined encoder knobs, lifted panels, switches with travel, deep keyboard keys, livelier 3D views with reflections and a comet trail, knob light trails) and the installer/DMG/icons rebranded to Paper.

---

## 3. Repository layout

```
CMakeLists.txt          JUCE 8.0.4 via FetchContent; version is in project(Hypernova VERSION x.y.z)
Source/
  PluginProcessor.*     parameters (APVTS), voices, MIDI handling, arp, presets, state, user wavetables,
                        mutate/randomise/section dice, limiter, sleep logic, publishModSources
  PluginEditor.*        the whole UI layout on a fixed 1280×986 base canvas that is scaled; menus;
                        theme application; update banner; drag-to-modulate plumbing
  Presets.h             factory presets (first bank) + shared bases, index cheat sheet at the top
  PresetsMore.h         second bank (keys, pads, experimental, ...), appended after the first
  PresetTrims.h         GENERATED loudness trims (scripts/level_presets.py)
  DSP/Wavetables.h      table generation (FFT mip levels), import from audio (fromAudio)
  DSP/Synth.h           voice engine: oscillators, warps, cross-mod, noise, filters, envelopes, LFOs,
                        mod matrix; modDestParam/modDestForParam map destinations to knobs
  DSP/Effects.h         effect chain (distortion, OTT, chorus, delay styles, reverb modes, EQ, width)
  DSP/ExtraFx.h         flanger, tape wear, gate/auto-pan, FX filter, pitch shifter
  UI/Style.h            fonts (bundled), themes (ThemeState/ThemeColour/LookSettings), look-and-feel,
                        knob styles (incl. cached machined bodies), panel styles
  UI/Components.h       knobs (mod rings, trails, right-click), views (wavetable, Sound Space, envelopes,
                        LFOs, filter), chips, toggles, keyboard (DeepKeyboard), icons
  DSP/Sampler.h         SampleData (guard-padded, cubic read), SamplerSettings, SamplePlayer (loops, reverse, fades),
                        pitch detection. Samples load on the message thread (HypernovaAudioProcessor::loadSample);
                        the audio thread reads an atomic pointer; replaced ones are retired for 3 s; FLAC in state
  UI/SamplerView.h      sampler waveform editor (flags, loop brackets, playhead, drop to load)
  UI/Dock.h             docking layout tree (splits and tabbed leaves): layout, drop targets, dividers, save/load
  UI/Widgets.h          Widget (panel + Spread responsive relayout + edit chrome), StackTabs, DockOverlay (all
                        drag/resize/drop interaction), WorkspaceStore
  UI/ToolWidgets.h      tool widgets (Scope, Loudness Meter, XY Pad, Mod Monitor, Macros, Pinboard) + WidgetLibrary
  EditorLayout.cpp      the widget host: catalogue, tool instances, default workspaces, operations, menus, undo
  UI/Cosmos.h           GPU backdrop shader (cosmos / neon horizon / plain) + CPU fallback
  UI/PresetBrowser.h    browser overlay
  UI/Updater.h          GitHub release check + download banner
Resources/Fonts/        bundled SIL OFL fonts (Space Grotesk, Space Mono, and alternates)
Tests/
  SmokeTest.cpp         audio test suite and tools (see section 5)
  UISnapshot.cpp        renders the editor to PNGs; --paintbench measures paint cost
  MakeFoundersPack.cpp  writes packaging/FOUNDERS PACK/*.hnpreset
  MakeInstallerArt.cpp  renders DMG background, installer card and icons (Paper style)
installer/app/          SwiftUI installer + uninstaller (same binary, HNMode=uninstall)
packaging/              distribution.xml, READ ME, release-notes.md, art, FOUNDERS PACK, pkg scripts
scripts/
  release.sh            one-command release (build, sign, notarize, staple, push, GitHub release)
  make-dmg.sh           the build/sign/notarize/DMG pipeline release.sh calls
  build-installer-app.sh
  level_presets.py      re-levels presets into PresetTrims.h
  publish-pack.sh       publishes a sound pack as a pack-<name> GitHub release
docs/                   THE DOWNLOAD PAGE (GitHub Pages serves main:/docs)
  index.html, style.css, app.js (release list, download button, packs), play.js (in-browser 3D
  wavetable you can play), assets/ (screenshots), favicon.svg
```

---

## 4. Rules that must not be broken

- **Session compatibility.** Plug-in codes stay `Arrw` / `ArBs`; the APVTS state tag stays `"ArrowBass"`. Choice lists that sessions store by index are **append-only**: wavetables, warps, filter types, mod sources, mod destinations, noise types, delay styles, reverb modes, chorus modes, distortion types, LFO sync, etc. Never reorder or remove entries. New parameters must default to the old behaviour (e.g. effect bypass params default ON).
- **Factory preset order is append-only too** (hosts store program numbers). New presets go at the end of `PresetsMore.h`. After adding or changing presets, run `python3 scripts/level_presets.py` to regenerate `PresetTrims.h`.
- **No ethnic or regional labels in preset names or categories** (the user asked for "Afro" tags to be removed). Genre names like amapiano, gqom, dancehall are fine.
- **Deployment target** must be set *before* `project()` in CMakeLists.txt (it is). Intel 10.13, arm64 11.0. The installer app targets macOS 12 — no newer SwiftUI APIs outside `#available`.
- **Audio thread:** no locks or allocations; UI reads audio state through atomics (`shownPos`, `shownModSource`, `scope`, …). User wavetables are kept alive in a cache for the processor's lifetime so the audio thread can hold raw pointers.
- **UI cost:** anything that repaints per frame must be cheap. Cache static layers as images keyed by size + theme version (`ThemeState::get().version`). Don't rebuild name lists or scan parameters per knob per frame. Measure with `UISnapshot <dir> --paintbench` and with `ps`/`sample` on the standalone.
- **Colours** always come from the theme (`Colours::…`, `Palette::…` are live proxies). Don't hard-code panel/text colours; check all five themes.
- **Pop-up menus** must call `menu.setLookAndFeel (&lookAndFeel)` (or the component's LAF), otherwise JUCE draws its default grey menus.
- **Copy voice** for the site and installer: dry, specific, lowercase, no hype adjectives ("pads the size of a galaxy" was rejected as cheesy). UI host-facing parameter names stay plain and descriptive.

---

## 5. How the work has been done (the process to keep)

1. **Understand, then ask only real decisions.** When the request was vague ("make it cooler"), the user was offered 2–4 concrete directions with trade-offs, then the chosen one was built. Otherwise act without asking.
2. **Measure before and after.** Every fidelity, click, loudness and CPU claim comes from a tool:
   - `./build/SmokeTest_artefacts/Release/SmokeTest` — full suite (renders all presets, timing, round-trip, pack, chords, arp, sleep/wake). Must print `ALL OK`.
   - `--bench` (audio CPU per preset), `--loudness` (feeds level_presets.py), `--lowend` (sub energy per bass), `--clicks [preset]` (note-change clicks), `--fidelity`, `--newfx` (every newer effect stays finite and audible), `--import <wav>` (wavetable import round trip), `--opentime` (table build time + checksum), `--note "<preset>" <note> <out.wav>`.
   - `./build/UISnapshot_artefacts/Release/UISnapshot <dir>` renders every deck page, the browser and the stereo view. Use `HYPERNOVA_THEME=<Paper|Cosmic|Blackout|Daylight|Vapor>` to force a theme and `HYPERNOVA_NO_UPDATE_CHECK=1` to stop network calls. `--paintbench` lists the most expensive components.
   - For real-world checks, open the standalone (`build/Hypernova_artefacts/Release/Standalone/Hypernova.app`) and click through every menu, tab and view; measure CPU with `ps -o %cpu= -p <pid>` and profile with `sample <pid> 4`.
3. **Look at every rendered screen yourself** in all five themes before calling UI work done (contact sheets were used). Screenshots render with the CPU backdrop; the plug-in uses the GPU shader.
4. **Commit small, with explanatory messages** (what changed and why, in plain prose). Every commit ends with a co-author line. Build, run SmokeTest, then commit.
5. **Tell the user plainly** what was verified and what wasn't (e.g. couldn't test on real macOS 12, couldn't hear audio).
6. **Send the DMG** to the user after each release (it's ~40 MB; over the 30 MB phone limit, so point them at the download page too).

Build commands:

```bash
cd ~/Hypernova
cmake -B build -DFETCHCONTENT_SOURCE_DIR_JUCE="/Users/erinarrow/ArrowEch /build/_deps/juce-src"   # first time; reuses ArrowEch's JUCE checkout
cmake --build build --config Release -j8          # builds everything and installs VST3/AU to ~/Library/Audio/Plug-Ins
HYPERNOVA_NO_UPDATE_CHECK=1 ./build/SmokeTest_artefacts/Release/SmokeTest
./build/UISnapshot_artefacts/Release/UISnapshot /tmp/hn-shots
auval -v aumu ArBs Arrw                           # AU validation
```

---

## 6. Releasing — exactly what to push at the end

Do all of this for every release:

1. **Bump the version** in `CMakeLists.txt`: `project(Hypernova VERSION x.y.z)`. The updater compares this against GitHub's latest tag, so a release without a bump won't reach users.
2. **Write `packaging/release-notes.md`** — it becomes the GitHub release body *and* the changelog on the download page. Keep the first paragraph (download/install line) and use short lowercase headings and bullets like the existing notes.
3. **If presets changed:** `python3 scripts/level_presets.py`, and if Founders Pack sounds changed: `./build/MakeFoundersPack_artefacts/Release/MakeFoundersPack "packaging/FOUNDERS PACK"`.
4. **If branding/art changed:** `./build/MakeInstallerArt_artefacts/Release/MakeInstallerArt .` and rebuild `packaging/art/AppIcon.icns` from `AppIcon.png` with `iconutil`.
5. **Update the download page (`docs/`)** — it is not automatic for content:
   - The download button, version/size/date line and the changelog update themselves from the GitHub releases API, so they need no edits.
   - **You must update by hand** anything describing features: the spec lists and copy in `docs/index.html`, and the screenshots in `docs/assets/` (render with UISnapshot in each theme, then convert to ~1600 px JPEGs; `theme_paper.jpg` is the hero, `theme_cosmic/blackout/daylight/vapor.jpg` fill "five looks"). If the number of themes, effects or sounds changes, change the numbers in the copy.
   - After editing `app.js`, `play.js` or `style.css`, bump the `?v=` query on their `<script>`/`<link>` tags in `index.html` so browsers pick up the change.
   - Check the page locally: `cd docs && python3 -m http.server 8791`, then open http://localhost:8791 — the download button must point at the new `.dmg`, the changelog must list the new release, and the browser console must be free of errors. (A stray apostrophe inside a single-quoted JS string once broke the whole script; run `node --check docs/app.js docs/play.js`.)
6. **Run the tests** (SmokeTest `ALL OK`, `--newfx` `ALL OK`) and look at fresh UISnapshot renders.
7. **Commit everything** (code, notes, docs, art).
8. **Release:** `./scripts/release.sh`. It builds the universal plug-ins and standalone, signs with Developer ID, builds the pkg, installer and uninstaller apps, notarizes and staples everything, makes the styled DMG, checks it with `spctl`, runs `git push origin HEAD`, and creates the GitHub release `v<version>` with the DMG attached and the notes from `packaging/release-notes.md`. It takes ~15–25 minutes (notarization is most of it).
   - Signing identities: `Developer ID Application: ERIN UCKUZULAR (C3NF86XPV5)` and `Developer ID Installer: ERIN UCKUZULAR (C3NF86XPV5)` (in the login keychain).
   - Notarization uses the keychain profile **`arrow-switch-notary`**, which is backed by an App Store Connect API key. If notarytool ever says the profile is missing, the owner has to recreate it (it needs their API key and issuer ID, or an app-specific password) — an AI agent should not handle those credentials itself.
9. **Verify:** `xcrun stapler validate dist/Hypernova-x.y.z.dmg`, `spctl -a -t open --context context:primary-signature -v dist/Hypernova-x.y.z.dmg` (expect "Notarized Developer ID"), `gh release view vx.y.z`, and reload https://erinuckuzular-ai.github.io/Hypernova/ to confirm it offers the new version.
10. **Send the DMG** to the user.

Sound packs (separate from app releases): `./scripts/publish-pack.sh "<folder of .hnpreset/.hnwt>" "one line about it"`. It creates or updates a `pack-<name>` release that is never marked latest, so the updater and the main download button keep pointing at Hypernova itself; the site's Sound Packs section lists it automatically.

---

## 7. Open work (in rough priority)

**The current source of truth is `ROADMAP.md`**: the full retained roadmap with a status for each item, plus the explicit exclusions (Flight Recorder, Groove Gravity, Pilot Mode, Tape Desk, Dub Desk, per-step parameter changes, performance scenes). Where the list below conflicts with it, ROADMAP.md wins; in particular, no step sequencer.

From the user's requests and research that haven't been done yet:

1. **More 3D / "edgy and fun"** — the user's latest direction. Ideas queued: a persistent modulation strip above the deck with live mini-scopes (the Pigments feature reviewers love most), an XY "orbiter" macro pad (drag a comet through a star field to morph many parameters), wavetable views reacting harder to modulation (ghost frame at the modulated position tinted by the source colour), hover-to-reveal targets in both directions, knob "click" feedback.
2. **Playing and control:** MIDI learn for knobs, MPE, velocity and mod-wheel curves. (The step sequencer is excluded; see ROADMAP.md.)
3. **From the feature research:** mod-matrix remap curves and drawable LFO shapes, a second filter with serial/parallel routing, unison spread modes, microtuning (MTS-ESP / Scala), envelope follower / sidechain as a mod source, A/B compare, reorderable effects chain, per-voice random / free-running phase, more filter models (ladder, diode). Formant shifter was requested but skipped (needs spectral processing).
4. **Preset browser:** tags, audition on hover, preset thumbnails.
5. **Verification gaps:** the installer app has not been run on a real macOS 12 machine; notes on the standalone's on-screen keyboard weren't audibly checked in the last session (automated audio tests pass); pop-out window behaviour in hosts other than the standalone is untested.
6. **Repo size:** a folder of test WAVs (~363 MB) was committed by mistake in 0.4.x and removed in `c81ecc2`, but it is still in git history, so clones are large. Cleaning it needs a history rewrite and force-push — only with the owner's explicit OK.

---

## 8. User preferences to respect

- Wants features and polish pushed hard ("so cool, edgy and fun — audio nerds should go crazy over it"), and wants it to feel like it competes with Teenage Engineering.
- Wants every update committed, released (notarized) and the DMG sent. The GitHub page must always offer the newest version.
- Cares a lot about CPU ("imagine this is on 10 tracks at once") and about things being bug-free ("make sure it's perfect").
- Dislikes cheesy copy and generic "AI-looking" fonts; prefers the Paper look and the dry lowercase voice.
- Sound is the priority: 808s must behave like 808s, log drums like the DX10 patch, basses heavy.
