#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <initializer_list>
#include <utility>
#include <vector>

// Factory presets. Values are in real parameter units (choice/bool params use their index).
// Anything not listed falls back to the parameter default, so every preset starts from Init.
// Values are applied in order, so with() can take a shared base and override parts of it.
// Output volume is not set here: PresetTrims.h levels every preset to the same loudness.
//
// Index cheat sheet:
//   tables  0 Analog 1 Sync Saw 2 Pulse 3 808 Body 4 Log Drum 5 Growl 6 Vowel 7 Digital 8 Fold 9 Harmonic 10 Metal 11 Scream
//           12 Rich Sine 13 Glass 14 Air
//   warp    0 Off 1 Sync 2 Bend 3 Mirror 4 PWM 5 Crush 6 FM (true FM from the other osc; 100% = index ~9.4)
//   filter  0 LP12 1 LP24 2 HP 3 BP 4 Notch 5 Dirty LP 6 Comb 7 Formant     sub 0 Sine 1 Tri 2 Square 3 Punch
//   lfo     0 Sine 1 Tri 2 Saw 3 Ramp 4 Square 5 S&H 6 Smooth  mode  0 Poly 1 Mono 2 Legato
//   sync    0 Free 1 8bar 2 4bar 3 2bar 4 1bar 5 1/2 6 1/4 7 1/8 8 1/16 9 1/32 10 1/4T 11 1/8T 12 1/16T 13 1/4D 14 1/8D
//   src     1 LFO1 2 LFO2 3 ModEnv 4 Vel 5 ModWheel 6 Note 7-10 Macro1-4 11 Random
//   dest    1 APos 2 BPos 3 AWarp 4 BWarp 5 ALvl 6 BLvl 7 Pitch 8 Cutoff 9 Res 10 Drive 11 Sub 12 Noise 13 ADet 14 BDet 15 Pan 16 Amp
//           17 LFO1 Rate 18 LFO2 Rate 19 FX Dist 20 FX OTT 21 FX Chorus 22 FX Delay 23 FX Reverb 24 FX Shimmer
//   dist    0 Tube 1 Hard 2 Fold 3 808 Clip 4 Rectify 5 Crush
//   delay   0 1/32 1 1/16 2 1/16D 3 1/8T 4 1/8 5 1/8D 6 1/4T 7 1/4 8 1/4D 9 1/2 10 1bar
//   chord   0 Off 1 Oct 2 Fifth 3 Power 4 Major 5 Minor 6 Sus2 7 Sus4 8 Maj7 9 Min7 10 Dom7 11 Min9 12 Maj9 13 Min11 14 House Min7 15 Open Minor 16 Dim7
//   arp     mode 0 Up 1 Down 2 Up/Down 3 Random 4 Played    rate 0 1/4 1 1/8 2 1/8T 3 1/16 4 1/16T 5 1/32
namespace ab
{

using PresetValues = std::vector<std::pair<const char*, float>>;
using Macros = std::array<const char*, 4>;

struct Preset
{
    const char* name;
    const char* category;
    PresetValues values;
    Macros macros { "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4" };
};

inline PresetValues with (const PresetValues& base, std::initializer_list<std::pair<const char*, float>> extra)
{
    PresetValues v (base);
    v.insert (v.end(), extra.begin(), extra.end());
    return v;
}

} // namespace ab

#include "PresetsMore.h"

namespace ab
{

inline const std::vector<Preset>& factoryPresets()
{
    //==========================================================================
    // Shared starting points

    // 808: saturating 808 body, legato slides, pitch punch. Macro 1 GRIT morphs the body, Macro 2 DARK closes the filter,
    // Macro 3 DIRT pushes the distortion.
    static const PresetValues base808
    {
        { "aTable", 3 }, { "aPos", 0.4f }, { "aLevel", 0.9f },
        { "dropAmt", 12 }, { "dropTime", 0.04f }, { "mode", 2 }, { "glide", 0.09f }, { "bendRange", 12 },
        { "ampA", 0.001f }, { "ampD", 2.5f }, { "ampS", 0.55f }, { "ampR", 0.25f },
        { "fltType", 1 }, { "cutoff", 20000 },
        { "mod1Src", 7 }, { "mod1Dest", 1 }, { "mod1Amt", 0.5f },
        { "mod2Src", 8 }, { "mod2Dest", 8 }, { "mod2Amt", -0.75f },
        { "mod3Src", 9 }, { "mod3Dest", 19 }, { "mod3Amt", 0.6f },
    };
    static const Macros m808 { "GRIT", "DARK", "DIRT", "MACRO 4" };

    // The amapiano log drum is FL Studio's Fruity DX10 "Log Drum" patch (mda DX10): a sine carrier, a sine
    // modulator at 2x, FM index ~9.6 collapsing to ~1.2 in about 4 ms, an octave down, ~0.5 s decay.
    // Osc A is the carrier (Rich Sine = DX10's carrier shape), Osc B is the modulator, kept near-silent.
    // Macro 1 KNOCK adds FM index, Macro 2 SUB adds a sine under it, Macro 3 DARK closes the filter, Macro 4 SPACE.
    static const PresetValues logBase
    {
        { "aTable", 12 }, { "aPos", 0.151f }, { "aOct", -1 }, { "aLevel", 0.9f }, { "aWarp", 6 }, { "aWarpAmt", 0.127f },
        { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0 }, { "bLevel", 0.001f }, { "bFilter", 0 },
        { "subOn", 1 }, { "subOct", -1 }, { "subLevel", 0 }, { "subFilter", 0 },
        { "fltType", 1 }, { "cutoff", 20000 }, { "velSens", 0.6f },
        { "ampA", 0.001f }, { "ampD", 0.51f }, { "ampS", 0 }, { "ampR", 0.57f },
        { "modA", 0.001f }, { "modD", 0.03f }, { "modS", 0 }, { "modR", 0.03f },
        { "mod1Src", 3 }, { "mod1Dest", 3 }, { "mod1Amt", 0.87f },
        { "mod2Src", 7 }, { "mod2Dest", 3 }, { "mod2Amt", 0.4f },
        { "mod3Src", 8 }, { "mod3Dest", 11 }, { "mod3Amt", 0.6f },
        { "mod4Src", 9 }, { "mod4Dest", 8 }, { "mod4Amt", -0.8f },
        { "mod5Src", 10 }, { "mod5Dest", 23 }, { "mod5Amt", 0.4f },
    };
    static const Macros mLog { "KNOCK", "SUB", "DARK", "SPACE" };

    // Mono bass: filter-shaped, Macro 1 OPEN, Macro 2 SHAPE (table position), Macro 3 DIRT, Macro 4 SPACE.
    static const PresetValues bassBase
    {
        { "mode", 1 }, { "ampD", 0.6f }, { "ampS", 0.7f }, { "ampR", 0.1f },
        { "fltType", 1 }, { "cutoff", 900 }, { "res", 0.15f },
        { "mod1Src", 7 }, { "mod1Dest", 8 }, { "mod1Amt", 0.6f },
        { "mod2Src", 8 }, { "mod2Dest", 1 }, { "mod2Amt", 0.5f },
        { "mod3Src", 9 }, { "mod3Dest", 19 }, { "mod3Amt", 0.6f },
        { "mod4Src", 10 }, { "mod4Dest", 23 }, { "mod4Amt", 0.4f },
    };
    static const Macros mBass { "OPEN", "SHAPE", "DIRT", "SPACE" };

    // Wobbles: LFO 1 drives table position and cutoff, sub underneath. Macro 1 RAGE, 2 SPEED (LFO rate), 3 DIRT, 4 OPEN.
    static const PresetValues wobBase
    {
        { "subOn", 1 }, { "subOct", -1 }, { "subLevel", 0.5f }, { "mode", 1 }, { "glide", 0.03f },
        { "fltType", 1 }, { "cutoff", 1200 }, { "res", 0.3f },
        { "lfo1Shape", 0 }, { "lfo1Sync", 7 },
        { "mod1Src", 1 }, { "mod1Dest", 1 }, { "mod1Amt", 0.4f },
        { "mod2Src", 1 }, { "mod2Dest", 8 }, { "mod2Amt", 0.3f },
        { "mod3Src", 7 }, { "mod3Dest", 1 }, { "mod3Amt", 0.4f },
        { "mod4Src", 8 }, { "mod4Dest", 17 }, { "mod4Amt", 0.34f },
        { "mod5Src", 9 }, { "mod5Dest", 19 }, { "mod5Amt", 0.6f },
        { "mod6Src", 10 }, { "mod6Dest", 8 }, { "mod6Amt", 0.5f },
        { "ott", 0.5f }, { "distType", 0 }, { "distDrive", 0.3f }, { "distMix", 0.4f },
    };
    static const Macros mWob { "RAGE", "SPEED", "DIRT", "OPEN" };

    // Poly stabs: short, one-key chords, a little room. Macro 1 OPEN, 2 DECAY-ish (mod env amount), 3 ECHO, 4 SPACE.
    static const PresetValues stabBase
    {
        { "mode", 0 }, { "chord", 9 }, { "retrig", 1 },
        { "ampA", 0.002f }, { "ampD", 0.35f }, { "ampS", 0.15f }, { "ampR", 0.2f },
        { "fltType", 1 }, { "cutoff", 1400 }, { "res", 0.2f }, { "fltEnv", 0.35f }, { "modD", 0.25f }, { "modS", 0 },
        { "mod1Src", 7 }, { "mod1Dest", 8 }, { "mod1Amt", 0.5f },
        { "mod2Src", 8 }, { "mod2Dest", 9 }, { "mod2Amt", 0.4f },
        { "mod3Src", 9 }, { "mod3Dest", 22 }, { "mod3Amt", 0.6f },
        { "mod4Src", 10 }, { "mod4Dest", 23 }, { "mod4Amt", 0.5f },
        { "verbSize", 0.45f }, { "verbMix", 0.15f },
    };
    static const Macros mStab { "OPEN", "BITE", "ECHO", "SPACE" };

    // Leads: legato, glide, delay + space. Macro 1 BRIGHT, 2 VIBRATO (mod wheel style), 3 ECHO, 4 SPACE.
    static const PresetValues leadBase
    {
        { "mode", 2 }, { "glide", 0.08f }, { "retrig", 0 },
        { "ampA", 0.004f }, { "ampD", 1.0f }, { "ampS", 0.85f }, { "ampR", 0.35f },
        { "fltType", 1 }, { "cutoff", 3500 }, { "res", 0.15f },
        { "lfo2Rate", 5.5f }, { "lfo2Fade", 0.5f },
        { "mod1Src", 7 }, { "mod1Dest", 8 }, { "mod1Amt", 0.5f },
        { "mod2Src", 2 }, { "mod2Dest", 7 }, { "mod2Amt", 0.012f },
        { "mod3Src", 9 }, { "mod3Dest", 22 }, { "mod3Amt", 0.5f },
        { "mod4Src", 10 }, { "mod4Dest", 23 }, { "mod4Amt", 0.5f },
        { "dlyTime", 5 }, { "dlyFb", 0.35f }, { "dlyMix", 0.18f }, { "verbSize", 0.6f }, { "verbMix", 0.22f },
    };
    static const Macros mLead { "BRIGHT", "MACRO 2", "ECHO", "SPACE" };

    // Soundscape: slow, wide, huge space. Macro 1 BLOOM opens the filter, 2 DRIFT moves the tables, 3 SHIMMER, 4 SPACE.
    static const PresetValues spaceBase
    {
        { "retrig", 0 }, { "velSens", 0.3f }, { "drift", 0.3f },
        { "ampA", 1.8f }, { "ampD", 4.0f }, { "ampS", 0.8f }, { "ampR", 5.0f },
        { "fltType", 0 }, { "cutoff", 2400 }, { "res", 0.15f },
        { "lfo1Rate", 0.07f }, { "lfo1Retrig", 0 }, { "lfo2Shape", 6 }, { "lfo2Rate", 0.18f }, { "lfo2Retrig", 0 },
        { "mod1Src", 1 }, { "mod1Dest", 8 }, { "mod1Amt", 0.25f },
        { "mod2Src", 2 }, { "mod2Dest", 1 }, { "mod2Amt", 0.25f },
        { "mod3Src", 7 }, { "mod3Dest", 8 }, { "mod3Amt", 0.6f },
        { "mod4Src", 8 }, { "mod4Dest", 1 }, { "mod4Amt", 0.5f },
        { "mod5Src", 9 }, { "mod5Dest", 24 }, { "mod5Amt", 0.6f },
        { "mod6Src", 10 }, { "mod6Dest", 23 }, { "mod6Amt", 0.4f },
        { "chorusMix", 0.35f }, { "verbSize", 0.9f }, { "verbMix", 0.55f }, { "verbShimmer", 0.35f }, { "width", 1.3f },
    };
    static const Macros mSpace { "BLOOM", "DRIFT", "SHIMMER", "SPACE" };

    // Dub siren: a square-ish tone swept by LFO 1 into a dark, long dub delay. Macro 1 SWEEP (depth), 2 SPEED,
    // 3 ECHO (throw), 4 SPACE.
    static const PresetValues sirenBase
    {
        { "aTable", 2 }, { "aPos", 0.05f }, { "mode", 1 }, { "retrig", 0 }, { "fltType", 0 }, { "cutoff", 3200 },
        { "ampA", 0.005f }, { "ampS", 1.0f }, { "ampR", 0.25f },
        { "lfo1Shape", 1 }, { "lfo1Rate", 2.5f }, { "lfo1Retrig", 1 },
        { "mod1Src", 1 }, { "mod1Dest", 7 }, { "mod1Amt", 0.5f },
        { "mod2Src", 7 }, { "mod2Dest", 7 }, { "mod2Amt", 0.0f },
        { "mod3Src", 8 }, { "mod3Dest", 17 }, { "mod3Amt", 0.34f },
        { "mod4Src", 9 }, { "mod4Dest", 22 }, { "mod4Amt", 0.6f },
        { "mod5Src", 10 }, { "mod5Dest", 23 }, { "mod5Amt", 0.5f },
        { "mod6Src", 7 }, { "mod6Dest", 1 }, { "mod6Amt", 0.0f },
        { "dlyTime", 5 }, { "dlyFb", 0.72f }, { "dlyTone", 0.35f }, { "dlyMix", 0.45f }, { "verbSize", 0.6f }, { "verbMix", 0.25f },
    };
    static const Macros mSiren { "SWEEP", "SPEED", "ECHO", "SPACE" };

    static const std::vector<Preset> basePresets
    {
        { "Init", "Init", {} },

        //================================================================ 808
        { "Rager 808", "808", with (base808, { { "aPos", 0.45f }, { "dropAmt", 14 }, { "dropTime", 0.045f }, { "ampD", 2.6f },
            { "distType", 0 }, { "distDrive", 0.45f }, { "distMix", 1 }, { "ott", 0.25f } }), m808 },
        { "Sicko Slide", "808", with (base808, { { "aPos", 0.25f }, { "dropAmt", 7 }, { "dropTime", 0.03f }, { "glide", 0.14f },
            { "ampD", 3.5f }, { "ampS", 0.7f }, { "ampR", 0.3f }, { "mod1Amt", 0.6f }, { "distType", 3 }, { "distDrive", 0.35f }, { "distMix", 1 } }), m808 },
        { "Astro Sub", "808", with (base808, { { "aTable", 0 }, { "aPos", 0 }, { "aLevel", 0.95f }, { "dropTime", 0.02f }, { "glide", 0.08f },
            { "ampD", 1.8f }, { "ampS", 0.4f }, { "ampR", 0.2f }, { "mod1Amt", 0.3f }, { "distType", 0 }, { "distDrive", 0.15f }, { "distMix", 0.6f } }), m808 },
        { "Night Growl 808", "808", with (base808, { { "aPos", 0.6f }, { "aLevel", 0.85f },
            { "bOn", 1 }, { "bTable", 5 }, { "bPos", 0.2f }, { "bLevel", 0.3f }, { "bOct", 1 }, { "glide", 0.1f }, { "ampD", 2.2f }, { "ampS", 0.5f },
            { "cutoff", 900 }, { "res", 0.2f }, { "fltEnv", 0.35f }, { "fltKey", 0.5f }, { "modD", 0.35f }, { "modS", 0.2f },
            { "mod1Dest", 2 }, { "mod1Amt", 0.7f }, { "mod2Amt", 0.5f }, { "distType", 2 }, { "distDrive", 0.25f }, { "distMix", 0.5f }, { "ott", 0.3f } }),
          { "GROWL", "OPEN", "DIRT", "MACRO 4" } },
        { "Blown Speaker 808", "808", with (base808, { { "aPos", 0.7f }, { "dropAmt", 16 }, { "dropTime", 0.035f }, { "glide", 0.07f },
            { "ampD", 2.0f }, { "ampS", 0.5f }, { "ampR", 0.2f }, { "distType", 1 }, { "distDrive", 0.7f }, { "distMix", 1 }, { "ott", 0.4f } }), m808 },
        { "Knock 808", "808", with (base808, { { "aPos", 0.35f }, { "subOn", 1 }, { "subShape", 3 }, { "subOct", 0 }, { "subLevel", 0.4f },
            { "dropAmt", 24 }, { "dropTime", 0.015f }, { "mode", 1 }, { "glide", 0 },
            { "ampD", 0.7f }, { "ampS", 0 }, { "ampR", 0.1f }, { "distType", 3 }, { "distDrive", 0.5f }, { "distMix", 1 } }), m808 },
        { "Drill Slide 808", "808", with (base808, { { "aPos", 0.5f }, { "dropTime", 0.03f }, { "glide", 0.11f },
            { "ampD", 4.0f }, { "ampS", 0.8f }, { "ampR", 0.3f }, { "distType", 3 }, { "distDrive", 0.45f }, { "distMix", 1 }, { "ott", 0.2f } }), m808 },
        { "Clean Long 808", "808", with (base808, { { "aTable", 0 }, { "aPos", 0.02f }, { "dropAmt", 5 }, { "dropTime", 0.02f },
            { "ampD", 5.0f }, { "ampS", 0.7f }, { "ampR", 0.35f }, { "mod1Amt", 0.25f }, { "distType", 0 }, { "distDrive", 0.1f }, { "distMix", 0.4f } }), m808 },
        { "Punchy Trap 808", "808", with (base808, { { "subOn", 1 }, { "subShape", 3 }, { "subOct", 0 }, { "subLevel", 0.35f },
            { "dropAmt", 24 }, { "dropTime", 0.02f }, { "ampD", 1.4f }, { "ampS", 0.3f },
            { "distType", 3 }, { "distDrive", 0.6f }, { "distMix", 1 }, { "ott", 0.3f } }), m808 },
        { "Phonk 808", "808", with (base808, { { "aPos", 0.85f }, { "dropAmt", 19 }, { "dropTime", 0.03f }, { "glide", 0.06f },
            { "ampD", 1.6f }, { "ampS", 0.45f }, { "cutoff", 6000 }, { "distType", 1 }, { "distDrive", 0.85f }, { "distMix", 1 }, { "ott", 0.5f } }), m808 },
        { "Rolling 808", "808", with (base808, { { "aPos", 0.55f }, { "glide", 0.12f }, { "cutoff", 800 }, { "fltKey", 0.6f },
            { "ampD", 3.0f }, { "ampS", 0.65f }, { "mod2Amt", 0.8f }, { "distType", 0 }, { "distDrive", 0.5f }, { "distMix", 0.9f } }),
          { "GRIT", "OPEN", "DIRT", "MACRO 4" } },
        { "Moog 808", "808", with (base808, { { "aPos", 0.3f }, { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0.66f }, { "bLevel", 0.3f },
            { "cutoff", 420 }, { "res", 0.25f }, { "fltEnv", 0.35f }, { "fltKey", 0.4f }, { "modD", 0.4f }, { "modS", 0.15f },
            { "subOn", 1 }, { "subOct", 0 }, { "subLevel", 0.35f }, { "mod2Amt", 0.7f }, { "distType", 0 }, { "distDrive", 0.3f }, { "distMix", 0.7f } }),
          { "GRIT", "OPEN", "DIRT", "MACRO 4" } },
        { "Hollow 808", "808", with (base808, { { "aTable", 12 }, { "aPos", 0.6f }, { "dropAmt", 10 },
            { "ampD", 2.4f }, { "ampS", 0.5f }, { "distType", 0 }, { "distDrive", 0.35f }, { "distMix", 0.8f } }), m808 },
        { "Glide Boom 808", "808", with (base808, { { "aPos", 0.35f }, { "glide", 0.24f }, { "dropAmt", 7 },
            { "ampD", 6.0f }, { "ampS", 0.75f }, { "ampR", 0.4f }, { "distType", 3 }, { "distDrive", 0.3f }, { "distMix", 1 } }), m808 },
        { "Reese 808", "808", with (base808, { { "aPos", 0.35f }, { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0.66f }, { "bUni", 3 }, { "bDetune", 0.22f },
            { "bLevel", 0.3f }, { "cutoff", 650 }, { "mod2Amt", 0.8f }, { "distType", 0 }, { "distDrive", 0.4f }, { "distMix", 0.8f } }),
          { "GRIT", "OPEN", "DIRT", "MACRO 4" } },
        { "Bitcrushed 808", "808", with (base808, { { "aPos", 0.5f }, { "distType", 5 }, { "distDrive", 0.45f }, { "distMix", 0.6f }, { "ott", 0.3f } }), m808 },
        { "Jersey Bounce 808", "808", with (base808, { { "aPos", 0.4f }, { "dropAmt", 24 }, { "dropTime", 0.025f }, { "mode", 1 }, { "glide", 0 },
            { "ampD", 0.5f }, { "ampS", 0 }, { "ampR", 0.08f }, { "distType", 3 }, { "distDrive", 0.55f }, { "distMix", 1 }, { "ott", 0.35f } }), m808 },
        { "UK Drill Bounce", "808", with (base808, { { "aPos", 0.5f }, { "glide", 0.08f }, { "dropTime", 0.035f },
            { "ampD", 2.2f }, { "ampS", 0.6f }, { "distType", 3 }, { "distDrive", 0.5f }, { "distMix", 1 } }), m808 },
        { "Memphis 808", "808", with (base808, { { "aPos", 0.6f }, { "dropAmt", 12 }, { "ampD", 1.8f }, { "cutoff", 5000 },
            { "distType", 5 }, { "distDrive", 0.3f }, { "distMix", 0.45f }, { "ott", 0.2f } }), m808 },
        { "Plugg 808", "808", with (base808, { { "aTable", 0 }, { "aPos", 0.08f }, { "dropAmt", 5 }, { "glide", 0.1f },
            { "ampD", 4.5f }, { "ampS", 0.7f }, { "distType", 2 }, { "distDrive", 0.2f }, { "distMix", 0.35f },
            { "verbMix", 0.08f }, { "verbSize", 0.4f } }), m808 },
        { "Stadium 808", "808", with (base808, { { "aPos", 0.45f }, { "ampD", 3.0f }, { "ott", 0.45f },
            { "distType", 0 }, { "distDrive", 0.4f }, { "distMix", 1 }, { "verbMix", 0.1f }, { "verbSize", 0.55f } }), m808 },
        { "Tough 808", "808", with (base808, { { "aPos", 0.75f }, { "dropAmt", 17 }, { "dropTime", 0.03f },
            { "ampD", 2.0f }, { "ampS", 0.55f }, { "distType", 4 }, { "distDrive", 0.35f }, { "distMix", 0.5f }, { "ott", 0.3f } }), m808 },
        { "Rage Slide 808", "808", with (base808, { { "aPos", 0.8f }, { "glide", 0.16f }, { "dropAmt", 12 },
            { "ampD", 4.0f }, { "ampS", 0.8f }, { "distType", 1 }, { "distDrive", 0.6f }, { "distMix", 1 }, { "ott", 0.4f } }), m808 },

        //================================================================ Log drum
        { "Classic Log", "Log Drum", with (logBase, {}), mLog },
        { "Slide Log", "Log Drum", with (logBase, { { "mode", 2 }, { "glide", 0.07f }, { "bendRange", 12 } }), mLog },
        { "Deep Log", "Log Drum", with (logBase, { { "aWarpAmt", 0.08f }, { "mod1Amt", 0.6f }, { "ampD", 0.7f }, { "subLevel", 0.3f } }), mLog },
        { "Knock Log", "Log Drum", with (logBase, { { "aWarpAmt", 0.2f }, { "modD", 0.05f }, { "ampD", 0.4f },
            { "distType", 0 }, { "distDrive", 0.4f }, { "distMix", 0.7f }, { "ott", 0.3f } }), mLog },
        { "Bright Log", "Log Drum", with (logBase, { { "aPos", 0.5f }, { "aWarpAmt", 0.16f }, { "mod1Amt", 0.84f }, { "modD", 0.045f } }), mLog },
        { "Private School Log", "Log Drum", with (logBase, { { "aWarpAmt", 0.08f }, { "mod1Amt", 0.5f }, { "ampD", 0.6f }, { "ampR", 0.5f },
            { "fltType", 0 }, { "cutoff", 900 }, { "chorusMix", 0.15f }, { "verbMix", 0.12f }, { "verbSize", 0.35f } }), mLog },
        { "Bacardi Log", "Log Drum", with (logBase, { { "aWarpAmt", 0.18f }, { "ampD", 0.35f }, { "ampR", 0.3f },
            { "distType", 3 }, { "distDrive", 0.5f }, { "distMix", 1 }, { "ott", 0.35f } }), mLog },
        { "Gqom Log", "Log Drum", with (logBase, { { "bOct", -1 }, { "aWarpAmt", 0.15f }, { "ampD", 0.4f },
            { "cutoff", 2500 }, { "verbMix", 0.15f }, { "verbSize", 0.6f }, { "distType", 0 }, { "distDrive", 0.3f }, { "distMix", 0.6f } }), mLog },
        { "Twang Log", "Log Drum", with (logBase, { { "modD", 0.18f }, { "mod1Amt", 0.75f }, { "ampD", 0.6f } }), mLog },
        { "Log Boom", "Log Drum", with (logBase, { { "subShape", 3 }, { "subLevel", 0.35f }, { "ampD", 1.2f }, { "ampR", 0.8f },
            { "distType", 0 }, { "distDrive", 0.3f }, { "distMix", 0.6f } }), mLog },
        { "Velvet Log", "Log Drum", with (logBase, { { "aWarpAmt", 0.06f }, { "mod1Amt", 0.55f }, { "modD", 0.02f }, { "ampD", 0.8f }, { "ampR", 0.8f },
            { "fltType", 0 }, { "cutoff", 1400 }, { "verbMix", 0.1f }, { "verbSize", 0.45f } }), mLog },
        { "Stadium Log", "Log Drum", with (logBase, { { "aWarpAmt", 0.16f }, { "ott", 0.35f }, { "distType", 0 }, { "distDrive", 0.35f },
            { "distMix", 0.6f }, { "dlyTime", 5 }, { "dlyFb", 0.25f }, { "dlyMix", 0.12f }, { "verbMix", 0.2f }, { "verbSize", 0.65f } }), mLog },
        { "Tech Log", "Log Drum", with (logBase, { { "bOct", 1 }, { "aWarpAmt", 0.08f }, { "mod1Amt", 0.6f }, { "ampD", 0.35f } }), mLog },
        { "Hollow Log", "Log Drum", with (logBase, { { "aPos", 0.7f }, { "ampD", 0.5f } }), mLog },
        { "Short Log", "Log Drum", with (logBase, { { "ampD", 0.22f }, { "ampR", 0.15f } }), mLog },
        { "Wide Log", "Log Drum", with (logBase, { { "chorusMix", 0.3f }, { "width", 1.4f }, { "verbMix", 0.1f }, { "verbSize", 0.4f } }), mLog },
        { "Muffled Log", "Log Drum", with (logBase, { { "fltType", 0 }, { "cutoff", 500 }, { "subLevel", 0.2f }, { "ampD", 0.6f } }), mLog },
        { "Fifth Ratio Log", "Log Drum", with (logBase, { { "bOct", 1 }, { "bSemi", -5 }, { "aWarpAmt", 0.1f }, { "mod1Amt", 0.7f } }), mLog },
        { "Log Stab", "Log Drum", with (logBase, { { "chord", 1 }, { "ampD", 0.45f }, { "verbMix", 0.12f }, { "verbSize", 0.4f } }), mLog },
        { "Dusty Log", "Log Drum", with (logBase, { { "distType", 5 }, { "distDrive", 0.25f }, { "distMix", 0.35f }, { "fltType", 0 }, { "cutoff", 3500 } }), mLog },

        //================================================================ Sub
        { "Deep House Sub", "Sub", with (bassBase, { { "aTable", 0 }, { "aPos", 0.15f }, { "glide", 0.02f },
            { "ampD", 0.35f }, { "ampS", 0.6f }, { "fltType", 0 }, { "cutoff", 600 }, { "res", 0.1f } }), mBass },
        { "Pure Sine Sub", "Sub", with (bassBase, { { "aTable", 0 }, { "aPos", 0 }, { "fltOn", 0 }, { "ampS", 1.0f }, { "ampR", 0.08f } }), mBass },
        { "Warm Tri Sub", "Sub", with (bassBase, { { "aTable", 0 }, { "aPos", 0.3f }, { "glide", 0.03f }, { "fltType", 0 },
            { "distType", 0 }, { "distDrive", 0.2f }, { "distMix", 0.4f } }), mBass },
        { "Garage Sub", "Sub", with (bassBase, { { "aTable", 0 }, { "aPos", 0.05f }, { "dropAmt", 5 }, { "dropTime", 0.02f },
            { "ampD", 0.4f }, { "ampS", 0.5f }, { "fltOn", 0 }, { "distType", 0 }, { "distDrive", 0.25f }, { "distMix", 0.5f } }), mBass },
        { "Afro House Sub", "Sub", with (bassBase, { { "aTable", 3 }, { "aPos", 0.15f }, { "glide", 0.03f },
            { "ampD", 0.5f }, { "ampS", 0.45f }, { "fltType", 0 }, { "cutoff", 1200 } }), mBass },
        { "Dub Techno Sub", "Sub", with (bassBase, { { "aTable", 0 }, { "aPos", 0.2f }, { "ampD", 0.5f }, { "ampS", 0.3f },
            { "fltType", 0 }, { "cutoff", 700 }, { "dlyTime", 5 }, { "dlyFb", 0.45f }, { "dlyMix", 0.2f }, { "verbMix", 0.15f }, { "verbSize", 0.5f } }), mBass },
        { "Round Sub", "Sub", with (bassBase, { { "aTable", 12 }, { "aPos", 0.3f }, { "fltType", 0 }, { "cutoff", 400 }, { "ampS", 0.9f } }), mBass },
        { "Punch Sub", "Sub", with (bassBase, { { "aOn", 0 }, { "subOn", 1 }, { "subOct", 0 }, { "subShape", 3 }, { "subLevel", 0.9f },
            { "dropAmt", 12 }, { "dropTime", 0.015f }, { "ampD", 0.4f }, { "ampS", 0.4f } }), mBass },
        { "Square Sub", "Sub", with (bassBase, { { "aTable", 2 }, { "aPos", 0 }, { "fltType", 1 }, { "cutoff", 220 }, { "ampS", 0.9f } }), mBass },
        { "Riddim Sub", "Sub", with (bassBase, { { "aTable", 0 }, { "aPos", 0.05f }, { "glide", 0.04f }, { "fltOn", 0 },
            { "ampD", 0.8f }, { "ampS", 0.6f }, { "distType", 0 }, { "distDrive", 0.2f }, { "distMix", 0.3f } }), mBass },

        //================================================================ Reese
        { "Reese Wide", "Reese", with (bassBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 5 }, { "aDetune", 0.35f }, { "aBlend", 0.7f },
            { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0.66f }, { "bUni", 3 }, { "bDetune", 0.18f }, { "bOct", -1 }, { "bLevel", 0.5f },
            { "mode", 2 }, { "glide", 0.05f }, { "retrig", 0 }, { "ampS", 1.0f }, { "cutoff", 1400 }, { "fltDrive", 0.3f }, { "lfo1Rate", 0.25f },
            { "mod5Src", 1 }, { "mod5Dest", 8 }, { "mod5Amt", 0.25f },
            { "chorusMix", 0.25f }, { "ott", 0.3f }, { "distType", 0 }, { "distDrive", 0.25f }, { "distMix", 0.4f } }), mBass },
        { "Neuro Reese", "Reese", with (bassBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.45f }, { "aBlend", 0.8f },
            { "bOn", 1 }, { "bTable", 5 }, { "bPos", 0.4f }, { "bOct", -1 }, { "bLevel", 0.45f },
            { "subOn", 1 }, { "subOct", -1 }, { "subLevel", 0.45f }, { "mode", 2 }, { "glide", 0.05f }, { "retrig", 0 }, { "ampS", 1.0f },
            { "fltType", 5 }, { "cutoff", 1200 }, { "res", 0.25f }, { "fltDrive", 0.35f }, { "lfo1Rate", 0.15f }, { "lfo1Retrig", 0 },
            { "mod5Src", 1 }, { "mod5Dest", 1 }, { "mod5Amt", 0.08f }, { "mod2Dest", 2 },
            { "ott", 0.5f }, { "distType", 0 }, { "distDrive", 0.35f }, { "distMix", 0.5f } }), mBass },
        { "Liquid Reese", "Reese", with (bassBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 5 }, { "aDetune", 0.25f },
            { "subOn", 1 }, { "subOct", -1 }, { "subLevel", 0.5f }, { "mode", 2 }, { "glide", 0.06f }, { "retrig", 0 }, { "ampS", 1.0f },
            { "fltType", 0 }, { "chorusMix", 0.35f }, { "verbMix", 0.1f }, { "verbSize", 0.5f } }), mBass },
        { "Dark Reese", "Reese", with (bassBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.3f },
            { "bOn", 1 }, { "bTable", 2 }, { "bPos", 0.2f }, { "bUni", 3 }, { "bDetune", 0.2f }, { "bLevel", 0.4f },
            { "mode", 2 }, { "glide", 0.05f }, { "retrig", 0 }, { "ampS", 1.0f },
            { "fltType", 5 }, { "cutoff", 600 }, { "res", 0.3f }, { "fltDrive", 0.4f },
            { "ott", 0.4f }, { "distType", 0 }, { "distDrive", 0.3f }, { "distMix", 0.5f } }), mBass },
        { "Hoover", "Reese", with (bassBase, { { "aTable", 2 }, { "aPos", 0.25f }, { "aWarp", 4 }, { "aWarpAmt", 0.3f }, { "aUni", 7 }, { "aDetune", 0.5f },
            { "aBlend", 0.8f }, { "dropAmt", -12 }, { "dropTime", 0.18f }, { "mode", 2 }, { "glide", 0.12f }, { "retrig", 0 }, { "ampS", 1.0f },
            { "fltType", 0 }, { "cutoff", 3500 }, { "lfo1Rate", 0.3f }, { "mod5Src", 1 }, { "mod5Dest", 3 }, { "mod5Amt", 0.25f },
            { "chorusMix", 0.6f }, { "ott", 0.3f } }), mBass },
        { "Jump Up Reese", "Reese", with (bassBase, { { "aTable", 5 }, { "aPos", 0.2f }, { "aUni", 5 }, { "aDetune", 0.3f },
            { "subOn", 1 }, { "subOct", -1 }, { "subLevel", 0.5f }, { "mode", 2 }, { "glide", 0.05f }, { "ampS", 1.0f },
            { "fltType", 1 }, { "cutoff", 1600 }, { "lfo1Sync", 7 }, { "mod5Src", 1 }, { "mod5Dest", 1 }, { "mod5Amt", 0.2f },
            { "ott", 0.5f }, { "distType", 0 }, { "distDrive", 0.4f }, { "distMix", 0.5f } }), mBass },
        { "Techno Reese", "Reese", with (bassBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 3 }, { "aDetune", 0.2f },
            { "ampD", 0.3f }, { "ampS", 0.4f }, { "fltType", 1 }, { "cutoff", 500 }, { "fltEnv", 0.3f }, { "modD", 0.15f }, { "modS", 0 },
            { "distType", 0 }, { "distDrive", 0.45f }, { "distMix", 0.7f } }), mBass },
        { "Detuned Saws", "Reese", with (bassBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.55f }, { "aBlend", 0.9f },
            { "mode", 2 }, { "glide", 0.04f }, { "ampS", 1.0f }, { "fltType", 1 }, { "cutoff", 2400 }, { "chorusMix", 0.3f }, { "width", 1.3f } }), mBass },
        { "Phase Reese", "Reese", with (bassBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 5 }, { "aDetune", 0.2f },
            { "subOn", 1 }, { "subOct", -1 }, { "subLevel", 0.45f }, { "mode", 2 }, { "ampS", 1.0f }, { "fltType", 4 }, { "cutoff", 900 }, { "res", 0.4f },
            { "lfo1Rate", 0.2f }, { "lfo1Retrig", 0 }, { "mod5Src", 1 }, { "mod5Dest", 8 }, { "mod5Amt", 0.5f },
            { "mod6Src", 1 }, { "mod6Dest", 13 }, { "mod6Amt", 0.15f } }), mBass },

        //================================================================ Growl & Wobble
        { "Growl Monster", "Growl & Wobble", with (wobBase, { { "aTable", 5 }, { "aPos", 0.3f }, { "aUni", 3 }, { "aDetune", 0.12f },
            { "res", 0.4f }, { "fltDrive", 0.4f }, { "lfo1Shape", 1 }, { "mod1Amt", 0.45f },
            { "distType", 2 }, { "distDrive", 0.35f }, { "distMix", 0.5f }, { "ott", 0.6f } }), mWob },
        { "Vowel Wob", "Growl & Wobble", with (wobBase, { { "aTable", 6 }, { "aPos", 0.2f }, { "aUni", 2 }, { "aDetune", 0.1f },
            { "fltType", 0 }, { "cutoff", 2500 }, { "lfo1Sync", 11 }, { "mod1Amt", 0.5f }, { "mod2Amt", 0.2f } }), mWob },
        { "Neuro Talk", "Growl & Wobble", with (wobBase, { { "aTable", 6 }, { "aPos", 0.4f }, { "aWarp", 6 }, { "aWarpAmt", 0.2f },
            { "bOn", 1 }, { "bTable", 10 }, { "bPos", 0.3f }, { "bLevel", 0.08f }, { "bFilter", 0 },
            { "fltType", 3 }, { "cutoff", 900 }, { "res", 0.35f }, { "lfo1Shape", 5 }, { "lfo1Sync", 8 },
            { "mod3Dest", 3 }, { "mod3Amt", 0.6f }, { "ott", 0.6f } }), mWob },
        { "Riddim Wobble", "Growl & Wobble", with (wobBase, { { "aTable", 7 }, { "aPos", 0.3f }, { "subLevel", 0.55f },
            { "fltType", 5 }, { "cutoff", 300 }, { "res", 0.35f }, { "fltDrive", 0.3f }, { "lfo1Shape", 4 },
            { "mod1Dest", 8 }, { "mod1Amt", 0.55f }, { "mod2Src", 0 }, { "ott", 0.6f }, { "distType", 1 } }), mWob },
        { "Yoi Wobble", "Growl & Wobble", with (wobBase, { { "aTable", 6 }, { "aPos", 0.0f }, { "aUni", 3 }, { "aDetune", 0.1f },
            { "fltType", 0 }, { "cutoff", 3000 }, { "res", 0.25f }, { "lfo1Shape", 3 }, { "lfo1Sync", 6 }, { "mod1Amt", 0.5f }, { "mod2Amt", 0.25f } }), mWob },
        { "Tearout Screech", "Growl & Wobble", with (wobBase, { { "aTable", 11 }, { "aPos", 0.4f }, { "aWarp", 6 }, { "aWarpAmt", 0.15f },
            { "aUni", 3 }, { "aDetune", 0.12f }, { "bOn", 1 }, { "bTable", 10 }, { "bPos", 0.5f }, { "bLevel", 0.05f }, { "bFilter", 0 },
            { "fltType", 3 }, { "cutoff", 1500 }, { "lfo1Shape", 5 }, { "lfo1Sync", 8 }, { "mod1Amt", 0.35f }, { "mod2Amt", 0.35f },
            { "mod3Dest", 3 }, { "mod3Amt", 0.5f }, { "ott", 0.7f }, { "distType", 2 } }), mWob },
        { "Wub 1/4T", "Growl & Wobble", with (wobBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 3 }, { "aDetune", 0.15f },
            { "fltType", 5 }, { "cutoff", 200 }, { "res", 0.5f }, { "lfo1Sync", 10 },
            { "mod1Dest", 8 }, { "mod1Amt", 0.65f }, { "mod2Src", 0 } }), mWob },
        { "Metal Growl", "Growl & Wobble", with (wobBase, { { "aTable", 10 }, { "aPos", 0.3f }, { "aUni", 2 }, { "aDetune", 0.1f },
            { "cutoff", 2500 }, { "res", 0.2f }, { "lfo1Shape", 1 }, { "lfo1Sync", 8 }, { "mod3Amt", 0.5f }, { "ott", 0.6f }, { "distType", 2 } }), mWob },
        { "Fold Wobble", "Growl & Wobble", with (wobBase, { { "aTable", 8 }, { "aPos", 0.3f }, { "aUni", 2 }, { "aDetune", 0.08f },
            { "cutoff", 2000 }, { "res", 0.15f }, { "mod1Amt", 0.35f } }), mWob },
        { "Talking Bass", "Growl & Wobble", with (wobBase, { { "aTable", 6 }, { "aPos", 0.5f }, { "glide", 0.04f },
            { "fltType", 3 }, { "cutoff", 1100 }, { "lfo1Shape", 6 }, { "lfo1Sync", 8 }, { "mod1Amt", 0.45f } }), mWob },
        { "Laser Wobble", "Growl & Wobble", with (wobBase, { { "aTable", 1 }, { "aPos", 0.2f }, { "aUni", 3 }, { "aDetune", 0.1f },
            { "lfo1Shape", 2 }, { "lfo1Sync", 7 }, { "mod1Amt", 0.6f }, { "mod2Amt", 0.4f }, { "ott", 0.6f } }), mWob },
        { "Sync Wobble", "Growl & Wobble", with (wobBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aWarp", 1 }, { "aWarpAmt", 0.2f },
            { "lfo1Sync", 7 }, { "mod1Dest", 3 }, { "mod1Amt", 0.5f }, { "mod3Dest", 3 }, { "mod3Amt", 0.4f } }), mWob },
        { "Crush Wobble", "Growl & Wobble", with (wobBase, { { "aTable", 7 }, { "aPos", 0.6f }, { "aWarp", 5 }, { "aWarpAmt", 0.2f },
            { "lfo1Shape", 4 }, { "lfo1Sync", 8 }, { "mod1Dest", 3 }, { "mod1Amt", 0.5f }, { "distType", 5 }, { "distMix", 0.4f } }), mWob },
        { "Robot Riddim", "Growl & Wobble", with (wobBase, { { "aTable", 2 }, { "aPos", 0.3f }, { "aWarp", 4 }, { "aWarpAmt", 0.4f },
            { "fltType", 5 }, { "cutoff", 400 }, { "res", 0.4f }, { "lfo1Shape", 4 }, { "lfo1Sync", 12 },
            { "mod1Dest", 8 }, { "mod1Amt", 0.5f }, { "mod2Dest", 3 }, { "mod2Amt", 0.3f }, { "ott", 0.6f } }), mWob },
        { "Comb Growl", "Growl & Wobble", with (wobBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 3 }, { "aDetune", 0.1f },
            { "fltType", 6 }, { "cutoff", 300 }, { "res", 0.75f }, { "lfo1Shape", 1 }, { "lfo1Sync", 7 },
            { "mod1Dest", 8 }, { "mod1Amt", 0.4f }, { "mod2Src", 0 } }), mWob },
        { "Formant Growl", "Growl & Wobble", with (wobBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 3 }, { "aDetune", 0.12f },
            { "fltType", 7 }, { "cutoff", 300 }, { "res", 0.6f }, { "lfo1Shape", 0 }, { "lfo1Sync", 7 },
            { "mod1Dest", 8 }, { "mod1Amt", 0.6f }, { "mod2Src", 0 }, { "ott", 0.6f } }), mWob },
        { "Chainsaw", "Growl & Wobble", with (wobBase, { { "aTable", 11 }, { "aPos", 0.7f }, { "aUni", 5 }, { "aDetune", 0.2f },
            { "fltType", 5 }, { "cutoff", 1800 }, { "lfo1Shape", 2 }, { "lfo1Sync", 9 }, { "mod1Amt", 0.25f }, { "mod2Amt", 0.25f },
            { "ott", 0.7f }, { "distType", 1 }, { "distMix", 0.6f } }), mWob },
        { "Dubstep Classic", "Growl & Wobble", with (wobBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 5 }, { "aDetune", 0.18f },
            { "bOn", 1 }, { "bTable", 2 }, { "bPos", 0.2f }, { "bLevel", 0.4f },
            { "fltType", 1 }, { "cutoff", 250 }, { "res", 0.45f }, { "lfo1Sync", 7 },
            { "mod1Dest", 8 }, { "mod1Amt", 0.7f }, { "mod2Src", 0 }, { "ott", 0.5f } }), mWob },

        //================================================================ Pluck & Stab (bass)
        { "Hollow Pluck Bass", "Pluck Bass", with (bassBase, { { "aTable", 2 }, { "aPos", 0.3f },
            { "ampD", 0.25f }, { "ampS", 0 }, { "cutoff", 300 }, { "res", 0.25f }, { "fltEnv", 0.5f }, { "modD", 0.15f }, { "modS", 0 } }), mBass },
        { "Funk Pluck", "Pluck Bass", with (bassBase, { { "aTable", 2 }, { "aPos", 0.1f }, { "glide", 0.02f },
            { "ampD", 0.35f }, { "ampS", 0.2f }, { "cutoff", 250 }, { "res", 0.35f }, { "fltEnv", 0.6f }, { "modD", 0.2f }, { "modS", 0 },
            { "mod5Src", 4 }, { "mod5Dest", 8 }, { "mod5Amt", 0.3f }, { "distType", 0 }, { "distDrive", 0.25f }, { "distMix", 0.4f } }), mBass },
        { "Rubber Bass", "Pluck Bass", with (bassBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "mode", 2 }, { "glide", 0.06f },
            { "dropAmt", 7 }, { "dropTime", 0.06f }, { "ampD", 0.5f }, { "ampS", 0.4f },
            { "cutoff", 350 }, { "res", 0.5f }, { "fltEnv", 0.45f }, { "modD", 0.25f }, { "modS", 0.1f },
            { "subOn", 1 }, { "subOct", -1 }, { "subLevel", 0.4f } }), mBass },
        { "Slap FM Bass", "Pluck Bass", with (bassBase, { { "aTable", 12 }, { "aPos", 0.3f }, { "aWarp", 6 }, { "aWarpAmt", 0.05f },
            { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0 }, { "bLevel", 0.001f }, { "bFilter", 0 },
            { "ampS", 0.25f }, { "fltOn", 0 }, { "modD", 0.08f }, { "modS", 0 },
            { "mod1Src", 3 }, { "mod1Dest", 3 }, { "mod1Amt", 0.6f }, { "mod5Src", 4 }, { "mod5Dest", 3 }, { "mod5Amt", 0.2f } }), mBass },
        { "Stab Bass", "Pluck Bass", with (bassBase, { { "aTable", 2 }, { "aPos", 0.2f }, { "aUni", 3 }, { "aDetune", 0.12f },
            { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0.66f }, { "bOct", -1 }, { "bLevel", 0.4f },
            { "ampD", 0.22f }, { "ampS", 0 }, { "cutoff", 600 }, { "res", 0.2f }, { "fltEnv", 0.5f }, { "modD", 0.12f }, { "modS", 0 }, { "ott", 0.3f } }), mBass },
        { "Digital Grit", "Pluck Bass", with (bassBase, { { "aTable", 7 }, { "aPos", 0.5f }, { "aWarp", 5 }, { "aWarpAmt", 0.3f },
            { "subOn", 1 }, { "subOct", -1 }, { "subLevel", 0.5f }, { "cutoff", 4000 },
            { "lfo1Shape", 5 }, { "lfo1Sync", 8 }, { "mod5Src", 1 }, { "mod5Dest", 3 }, { "mod5Amt", 0.3f },
            { "distType", 5 }, { "distDrive", 0.3f }, { "distMix", 0.5f } }), mBass },
        { "Comb Pluck Bass", "Pluck Bass", with (bassBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "ampD", 0.4f }, { "ampS", 0 },
            { "fltType", 6 }, { "cutoff", 110 }, { "res", 0.8f }, { "fltKey", 1.0f }, { "fltEnv", 0.2f }, { "modD", 0.1f } }), mBass },
        { "Wood Pluck Bass", "Pluck Bass", with (bassBase, { { "aTable", 4 }, { "aPos", 0.5f }, { "ampD", 0.45f }, { "ampS", 0 },
            { "cutoff", 350 }, { "res", 0.4f }, { "fltEnv", 0.55f }, { "fltKey", 0.6f }, { "modD", 0.08f }, { "modS", 0 } }), mBass },
        { "Round Pluck Bass", "Pluck Bass", with (bassBase, { { "aTable", 0 }, { "aPos", 0.33f }, { "ampD", 0.6f }, { "ampS", 0.1f },
            { "fltType", 0 }, { "cutoff", 250 }, { "fltEnv", 0.4f }, { "modD", 0.2f }, { "modS", 0 } }), mBass },

        //================================================================ House & Garage (bass)
        { "Organ Bass", "House Bass", with (bassBase, { { "aTable", 9 }, { "aPos", 0.05f },
            { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0 }, { "bOct", -1 }, { "bLevel", 0.5f },
            { "ampD", 0.35f }, { "ampS", 0.6f }, { "fltType", 0 }, { "cutoff", 2500 } }), mBass },
        { "Garage Bass", "House Bass", with (bassBase, { { "aTable", 1 }, { "aPos", 0.15f }, { "mode", 2 }, { "glide", 0.05f },
            { "dropAmt", 5 }, { "dropTime", 0.03f }, { "ampD", 0.4f }, { "ampS", 0.55f },
            { "cutoff", 700 }, { "res", 0.2f }, { "fltEnv", 0.3f }, { "modD", 0.2f }, { "subOn", 1 }, { "subOct", -1 }, { "subLevel", 0.45f } }), mBass },
        { "Speed Garage Bass", "House Bass", with (bassBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 3 }, { "aDetune", 0.25f },
            { "subOn", 1 }, { "subOct", -1 }, { "subLevel", 0.5f }, { "mode", 2 }, { "glide", 0.05f },
            { "fltType", 3 }, { "cutoff", 700 }, { "res", 0.45f }, { "lfo1Sync", 6 }, { "mod5Src", 1 }, { "mod5Dest", 8 }, { "mod5Amt", 0.3f },
            { "mod6Src", 1 }, { "mod6Dest", 7 }, { "mod6Amt", 0.02f }, { "distType", 0 }, { "distDrive", 0.35f }, { "distMix", 0.5f } }), mBass },
        { "Tech House Roller", "House Bass", with (bassBase, { { "aTable", 0 }, { "aPos", 0.66f },
            { "ampD", 0.25f }, { "ampS", 0.3f }, { "cutoff", 260 }, { "res", 0.3f }, { "fltEnv", 0.5f }, { "modD", 0.12f }, { "modS", 0 },
            { "subOn", 1 }, { "subOct", -1 }, { "subLevel", 0.4f }, { "distType", 0 }, { "distDrive", 0.35f }, { "distMix", 0.6f } }), mBass },
        { "Afro House Bass", "House Bass", with (bassBase, { { "aTable", 12 }, { "aPos", 0.5f }, { "mode", 2 }, { "glide", 0.05f },
            { "ampD", 0.5f }, { "ampS", 0.35f }, { "fltType", 0 }, { "cutoff", 700 }, { "fltEnv", 0.25f }, { "modD", 0.2f } }), mBass },
        { "Minimal Blip", "House Bass", with (bassBase, { { "aTable", 0 }, { "aPos", 0 }, { "aWarp", 6 }, { "aWarpAmt", 0.0f },
            { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0 }, { "bOct", 1 }, { "bLevel", 0.001f }, { "bFilter", 0 },
            { "ampD", 0.18f }, { "ampS", 0 }, { "fltOn", 0 }, { "modD", 0.05f }, { "modS", 0 },
            { "mod1Src", 3 }, { "mod1Dest", 3 }, { "mod1Amt", 0.4f }, { "dlyTime", 2 }, { "dlyFb", 0.3f }, { "dlyMix", 0.15f } }), mBass },
        { "Acid Snake", "House Bass", with (bassBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "glide", 0.06f },
            { "ampD", 0.5f }, { "ampS", 0.6f }, { "fltType", 5 }, { "cutoff", 350 }, { "res", 0.75f }, { "fltEnv", 0.55f }, { "fltKey", 0.4f },
            { "modD", 0.18f }, { "modS", 0 }, { "mod5Src", 4 }, { "mod5Dest", 8 }, { "mod5Amt", 0.25f },
            { "distType", 0 }, { "distDrive", 0.4f }, { "distMix", 0.6f }, { "dlyTime", 5 }, { "dlyMix", 0.15f }, { "dlyFb", 0.4f } }), mBass },
        { "Dean Moog Bass", "House Bass", with (bassBase, { { "aTable", 0 }, { "aPos", 0.66f },
            { "bOn", 1 }, { "bTable", 2 }, { "bPos", 0.1f }, { "bOct", -1 }, { "bLevel", 0.55f }, { "mode", 2 }, { "glide", 0.07f },
            { "ampD", 1.0f }, { "ampS", 0.8f }, { "cutoff", 500 }, { "res", 0.3f }, { "fltEnv", 0.45f }, { "fltKey", 0.3f },
            { "modD", 0.3f }, { "modS", 0.2f }, { "distType", 0 }, { "distDrive", 0.2f }, { "distMix", 0.6f } }), mBass },
        { "Sync Screamer", "House Bass", with (bassBase, { { "aTable", 1 }, { "aPos", 0.2f }, { "aUni", 3 }, { "aDetune", 0.1f },
            { "subOn", 1 }, { "subOct", -1 }, { "subLevel", 0.55f }, { "glide", 0.04f }, { "cutoff", 3000 }, { "res", 0.2f },
            { "modD", 0.4f }, { "modS", 0.2f }, { "mod5Src", 3 }, { "mod5Dest", 1 }, { "mod5Amt", 0.6f },
            { "distType", 1 }, { "distDrive", 0.3f }, { "distMix", 0.5f }, { "ott", 0.45f } }), mBass },
        { "Deep Tech Bass", "House Bass", with (bassBase, { { "aTable", 12 }, { "aPos", 0.8f }, { "ampD", 0.3f }, { "ampS", 0.2f },
            { "fltType", 0 }, { "cutoff", 400 }, { "fltEnv", 0.3f }, { "modD", 0.1f }, { "modS", 0 },
            { "subOn", 1 }, { "subOct", -1 }, { "subLevel", 0.35f }, { "dlyTime", 1 }, { "dlyFb", 0.25f }, { "dlyMix", 0.1f } }), mBass },
        { "Jackin Bass", "House Bass", with (bassBase, { { "aTable", 2 }, { "aPos", 0.05f }, { "ampD", 0.2f }, { "ampS", 0.25f },
            { "cutoff", 450 }, { "res", 0.35f }, { "fltEnv", 0.45f }, { "modD", 0.1f }, { "modS", 0 },
            { "distType", 0 }, { "distDrive", 0.3f }, { "distMix", 0.5f } }), mBass },
        { "Chicago Acid", "House Bass", with (bassBase, { { "aTable", 2 }, { "aPos", 0.0f }, { "glide", 0.07f },
            { "ampD", 0.4f }, { "ampS", 0.5f }, { "fltType", 5 }, { "cutoff", 280 }, { "res", 0.85f }, { "fltEnv", 0.6f },
            { "modD", 0.14f }, { "modS", 0 }, { "mod5Src", 4 }, { "mod5Dest", 8 }, { "mod5Amt", 0.3f },
            { "distType", 0 }, { "distDrive", 0.35f }, { "distMix", 0.5f } }), mBass },
        { "Bouncy House Bass", "House Bass", with (bassBase, { { "aTable", 0 }, { "aPos", 0.4f }, { "dropAmt", 7 }, { "dropTime", 0.04f },
            { "ampD", 0.3f }, { "ampS", 0.2f }, { "cutoff", 800 }, { "fltEnv", 0.3f }, { "modD", 0.15f }, { "modS", 0 } }), mBass },

        //================================================================ House stabs (one-key chords)
        { "M1 Organ Stab", "House Stabs", with (stabBase, { { "chord", 0 }, { "aTable", 9 }, { "aPos", 0.15f },
            { "bOn", 1 }, { "bTable", 9 }, { "bPos", 0.45f }, { "bOct", 1 }, { "bLevel", 0.35f },
            { "ampD", 0.3f }, { "ampS", 0.35f }, { "fltType", 0 }, { "cutoff", 5000 }, { "fltEnv", 0 } }), mStab },
        { "Classic House Chord", "House Stabs", with (stabBase, { { "chord", 14 }, { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 3 }, { "aDetune", 0.12f },
            { "bOn", 1 }, { "bTable", 2 }, { "bPos", 0.2f }, { "bLevel", 0.4f } }), mStab },
        { "Deep House Chord", "House Stabs", with (stabBase, { { "chord", 11 }, { "aTable", 12 }, { "aPos", 0.3f }, { "aWarp", 6 }, { "aWarpAmt", 0.03f },
            { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0 }, { "bOct", 1 }, { "bLevel", 0.001f }, { "bFilter", 0 },
            { "ampD", 1.2f }, { "ampS", 0.2f }, { "ampR", 0.5f }, { "modD", 0.2f }, { "mod5Src", 3 }, { "mod5Dest", 3 }, { "mod5Amt", 0.2f },
            { "fltType", 0 }, { "cutoff", 2400 }, { "fltEnv", 0.2f }, { "chorusMix", 0.35f }, { "verbMix", 0.2f } }), mStab },
        { "Dub Techno Chord", "House Stabs", with (stabBase, { { "chord", 15 }, { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 3 }, { "aDetune", 0.1f },
            { "bOn", 1 }, { "bTable", 2 }, { "bPos", 0.3f }, { "bLevel", 0.35f }, { "cutoff", 700 }, { "res", 0.3f }, { "fltEnv", 0.3f },
            { "dlyTime", 5 }, { "dlyFb", 0.62f }, { "dlyTone", 0.35f }, { "dlyMix", 0.35f }, { "verbMix", 0.3f }, { "verbSize", 0.7f } }), mStab },
        { "Rave Stab", "House Stabs", with (stabBase, { { "chord", 5 }, { "aTable", 2 }, { "aPos", 0.3f }, { "aWarp", 4 }, { "aWarpAmt", 0.3f },
            { "aUni", 7 }, { "aDetune", 0.4f }, { "ampD", 0.25f }, { "cutoff", 3500 }, { "ott", 0.4f } }), mStab },
        { "Detroit Stab", "House Stabs", with (stabBase, { { "chord", 9 }, { "aTable", 2 }, { "aPos", 0.15f }, { "aUni", 3 }, { "aDetune", 0.1f },
            { "cutoff", 2000 }, { "fltEnv", 0.4f }, { "ampD", 0.3f }, { "dlyTime", 3 }, { "dlyFb", 0.35f }, { "dlyMix", 0.15f } }), mStab },
        { "Garage Organ Stab", "House Stabs", with (stabBase, { { "chord", 9 }, { "aTable", 9 }, { "aPos", 0.25f }, { "ampD", 0.2f }, { "ampS", 0.1f },
            { "fltType", 0 }, { "cutoff", 3500 }, { "fltEnv", 0.1f } }), mStab },
        { "Piano House Stab", "House Stabs", with (stabBase, { { "chord", 4 }, { "aTable", 12 }, { "aPos", 0.45f }, { "aWarp", 6 }, { "aWarpAmt", 0.06f },
            { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0 }, { "bOct", 1 }, { "bLevel", 0.001f }, { "bFilter", 0 },
            { "ampD", 1.4f }, { "ampS", 0.0f }, { "ampR", 0.4f }, { "modD", 0.35f }, { "mod5Src", 3 }, { "mod5Dest", 3 }, { "mod5Amt", 0.4f },
            { "fltType", 0 }, { "cutoff", 6000 }, { "fltEnv", 0 }, { "verbMix", 0.18f } }), mStab },
        { "French Filter Stab", "House Stabs", with (stabBase, { { "chord", 8 }, { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 5 }, { "aDetune", 0.15f },
            { "ampD", 0.4f }, { "ampS", 0.4f }, { "cutoff", 700 }, { "fltEnv", 0.2f }, { "lfo1Sync", 4 }, { "lfo1Retrig", 0 },
            { "mod5Src", 1 }, { "mod5Dest", 8 }, { "mod5Amt", 0.5f }, { "distType", 0 }, { "distDrive", 0.3f }, { "distMix", 0.5f } }), mStab },
        { "Glass Chord Pluck", "House Stabs", with (stabBase, { { "chord", 11 }, { "aTable", 13 }, { "aPos", 0.2f }, { "ampD", 0.5f }, { "ampS", 0 },
            { "fltType", 0 }, { "cutoff", 4000 }, { "verbMix", 0.25f }, { "verbShimmer", 0.2f } }), mStab },
        { "Minimal Chord Blip", "House Stabs", with (stabBase, { { "chord", 6 }, { "aTable", 0 }, { "aPos", 0.1f }, { "ampD", 0.12f }, { "ampS", 0 },
            { "fltOn", 0 }, { "dlyTime", 5 }, { "dlyFb", 0.45f }, { "dlyMix", 0.25f } }), mStab },
        { "Jazzy Rhodes Chord", "House Stabs", with (stabBase, { { "chord", 12 }, { "aTable", 12 }, { "aPos", 0.2f }, { "aWarp", 6 }, { "aWarpAmt", 0.04f },
            { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0 }, { "bOct", 0 }, { "bLevel", 0.001f }, { "bFilter", 0 },
            { "ampD", 2.0f }, { "ampS", 0.1f }, { "ampR", 0.5f }, { "modD", 0.3f }, { "mod5Src", 3 }, { "mod5Dest", 3 }, { "mod5Amt", 0.25f },
            { "fltType", 0 }, { "cutoff", 3000 }, { "fltEnv", 0 }, { "chorusMix", 0.4f }, { "verbMix", 0.2f } }), mStab },
        { "Tech House Stab", "House Stabs", with (stabBase, { { "chord", 15 }, { "aTable", 7 }, { "aPos", 0.2f }, { "ampD", 0.15f }, { "ampS", 0 },
            { "fltType", 3 }, { "cutoff", 1200 }, { "res", 0.35f } }), mStab },
        { "Soulful Keys Stab", "House Stabs", with (stabBase, { { "chord", 13 }, { "aTable", 9 }, { "aPos", 0.1f }, { "ampD", 0.6f }, { "ampS", 0.25f },
            { "fltType", 0 }, { "cutoff", 2800 }, { "chorusMix", 0.3f } }), mStab },
        { "Hardcore Stab", "House Stabs", with (stabBase, { { "chord", 5 }, { "aTable", 11 }, { "aPos", 0.3f }, { "aUni", 5 }, { "aDetune", 0.2f },
            { "ampD", 0.3f }, { "cutoff", 5000 }, { "distType", 1 }, { "distDrive", 0.4f }, { "distMix", 0.5f }, { "ott", 0.4f } }), mStab },
        { "Trance Gate Stab", "House Stabs", with (stabBase, { { "chord", 5 }, { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.35f },
            { "ampD", 2.0f }, { "ampS", 0.8f }, { "cutoff", 4000 }, { "fltEnv", 0 }, { "lfo1Shape", 4 }, { "lfo1Sync", 8 }, { "lfo1Retrig", 0 },
            { "mod5Src", 1 }, { "mod5Dest", 16 }, { "mod5Amt", -0.5f }, { "verbMix", 0.3f } }), mStab },
        { "Gospel House Organ", "House Stabs", with (stabBase, { { "chord", 12 }, { "aTable", 9 }, { "aPos", 0.3f }, { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0.3f },
            { "bLevel", 0.35f }, { "ampD", 1.0f }, { "ampS", 0.7f }, { "fltType", 0 }, { "cutoff", 3500 }, { "fltEnv", 0 },
            { "lfo2Rate", 6.0f }, { "mod5Src", 2 }, { "mod5Dest", 7 }, { "mod5Amt", 0.006f }, { "chorusMix", 0.4f } }), mStab },
        { "Chord Memory Stab", "House Stabs", with (stabBase, { { "chord", 9 }, { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 2 }, { "aDetune", 0.08f },
            { "ampD", 0.22f }, { "ampS", 0 }, { "cutoff", 900 }, { "res", 0.35f }, { "fltEnv", 0.5f }, { "modD", 0.15f } }), mStab },

        //================================================================ Techno & Euro
        { "Euro Supersaw", "Techno & Euro", with (leadBase, { { "mode", 0 }, { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.45f },
            { "aBlend", 0.85f }, { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0.66f }, { "bOct", 1 }, { "bUni", 5 }, { "bDetune", 0.35f }, { "bLevel", 0.35f },
            { "cutoff", 6000 }, { "verbMix", 0.3f }, { "dlyMix", 0.2f }, { "width", 1.3f } }), mLead },
        { "Eurodance Lead", "Techno & Euro", with (leadBase, { { "aTable", 2 }, { "aPos", 0.2f }, { "aUni", 5 }, { "aDetune", 0.25f },
            { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0.66f }, { "bUni", 3 }, { "bDetune", 0.2f }, { "bLevel", 0.5f }, { "glide", 0.03f },
            { "cutoff", 5000 } }), mLead },
        { "Donk Bass", "Techno & Euro", with (bassBase, { { "aTable", 12 }, { "aPos", 0.3f }, { "aWarp", 6 }, { "aWarpAmt", 0.05f },
            { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0 }, { "bOct", 1 }, { "bLevel", 0.001f }, { "bFilter", 0 },
            { "dropAmt", 12 }, { "dropTime", 0.04f }, { "ampD", 0.18f }, { "ampS", 0 }, { "fltOn", 0 },
            { "modD", 0.06f }, { "modS", 0 }, { "mod1Src", 3 }, { "mod1Dest", 3 }, { "mod1Amt", 0.8f },
            { "distType", 0 }, { "distDrive", 0.3f }, { "distMix", 0.6f } }), mBass },
        { "Hard Techno Rumble", "Techno & Euro", with (bassBase, { { "aTable", 0 }, { "aPos", 0.3f }, { "ampD", 0.35f }, { "ampS", 0 },
            { "fltType", 5 }, { "cutoff", 180 }, { "distType", 0 }, { "distDrive", 0.6f }, { "distMix", 1 },
            { "dlyTime", 1 }, { "dlyFb", 0.3f }, { "dlyTone", 0.2f }, { "dlyMix", 0.2f }, { "verbSize", 0.55f }, { "verbMix", 0.45f } }), mBass },
        { "Berlin Bass", "Techno & Euro", with (bassBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "ampD", 0.2f }, { "ampS", 0.1f },
            { "cutoff", 300 }, { "fltEnv", 0.35f }, { "modD", 0.12f }, { "modS", 0 }, { "distType", 0 }, { "distDrive", 0.4f }, { "distMix", 0.6f } }), mBass },
        { "Acid 303 Squelch", "Techno & Euro", with (bassBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "glide", 0.05f },
            { "ampD", 0.3f }, { "ampS", 0.5f }, { "fltType", 5 }, { "cutoff", 300 }, { "res", 0.88f }, { "fltEnv", 0.6f },
            { "modD", 0.12f }, { "modS", 0 }, { "mod5Src", 4 }, { "mod5Dest", 8 }, { "mod5Amt", 0.35f },
            { "distType", 0 }, { "distDrive", 0.45f }, { "distMix", 0.6f }, { "dlyTime", 5 }, { "dlyFb", 0.4f }, { "dlyMix", 0.15f } }), mBass },
        { "Acid Square", "Techno & Euro", with (bassBase, { { "aTable", 2 }, { "aPos", 0.0f }, { "glide", 0.05f },
            { "ampD", 0.3f }, { "ampS", 0.5f }, { "fltType", 5 }, { "cutoff", 260 }, { "res", 0.8f }, { "fltEnv", 0.55f },
            { "modD", 0.15f }, { "modS", 0 }, { "distType", 0 }, { "distDrive", 0.4f }, { "distMix", 0.5f } }), mBass },
        { "Rave Hoover Lead", "Techno & Euro", with (leadBase, { { "mode", 0 }, { "aTable", 2 }, { "aPos", 0.25f }, { "aWarp", 4 }, { "aWarpAmt", 0.35f },
            { "aUni", 7 }, { "aDetune", 0.5f }, { "dropAmt", -12 }, { "dropTime", 0.15f }, { "cutoff", 4000 }, { "chorusMix", 0.6f } }), mLead },
        { "Trance Pluck", "Techno & Euro", with (stabBase, { { "chord", 0 }, { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 5 }, { "aDetune", 0.25f },
            { "ampD", 0.25f }, { "ampS", 0 }, { "cutoff", 900 }, { "fltEnv", 0.5f }, { "modD", 0.2f },
            { "dlyTime", 5 }, { "dlyFb", 0.4f }, { "dlyMix", 0.25f }, { "verbMix", 0.25f } }), mStab },
        { "Gated Trance Pad", "Techno & Euro", with (spaceBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.35f },
            { "ampA", 0.02f }, { "cutoff", 3500 }, { "lfo1Shape", 4 }, { "lfo1Sync", 8 }, { "lfo1Retrig", 0 },
            { "mod1Dest", 16 }, { "mod1Amt", -0.6f }, { "verbShimmer", 0.1f }, { "verbMix", 0.35f } }), mSpace },
        { "Industrial Stab", "Techno & Euro", with (stabBase, { { "chord", 3 }, { "aTable", 10 }, { "aPos", 0.5f }, { "ampD", 0.25f },
            { "cutoff", 3000 }, { "distType", 2 }, { "distDrive", 0.5f }, { "distMix", 0.6f }, { "verbMix", 0.3f }, { "verbSize", 0.7f } }), mStab },
        { "Schranz Lead", "Techno & Euro", with (leadBase, { { "aTable", 11 }, { "aPos", 0.5f }, { "aUni", 5 }, { "aDetune", 0.2f },
            { "cutoff", 5000 }, { "distType", 1 }, { "distDrive", 0.6f }, { "distMix", 0.7f }, { "ott", 0.5f } }), mLead },
        { "Techno Blip", "Techno & Euro", with (stabBase, { { "chord", 0 }, { "aTable", 0 }, { "aPos", 0.05f }, { "ampD", 0.08f }, { "ampS", 0 },
            { "fltOn", 0 }, { "dlyTime", 2 }, { "dlyFb", 0.5f }, { "dlyMix", 0.3f } }), mStab },
        { "Peak Time Arp", "Techno & Euro", with (stabBase, { { "chord", 0 }, { "arpOn", 1 }, { "arpMode", 0 }, { "arpRate", 3 }, { "arpOct", 2 }, { "arpGate", 0.5f },
            { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 3 }, { "aDetune", 0.15f }, { "ampD", 0.18f }, { "ampS", 0 },
            { "cutoff", 1200 }, { "fltEnv", 0.4f }, { "dlyTime", 5 }, { "dlyFb", 0.35f }, { "dlyMix", 0.2f } }), mStab },
        { "Euro Pizz", "Techno & Euro", with (stabBase, { { "chord", 0 }, { "aTable", 2 }, { "aPos", 0.35f }, { "ampD", 0.15f }, { "ampS", 0 },
            { "fltType", 0 }, { "cutoff", 4000 }, { "fltEnv", 0 }, { "verbMix", 0.25f } }), mStab },
        { "Melodic Techno Lead", "Techno & Euro", with (leadBase, { { "aTable", 1 }, { "aPos", 0.2f }, { "aUni", 3 }, { "aDetune", 0.1f },
            { "lfo1Rate", 0.15f }, { "lfo1Retrig", 0 }, { "mod5Src", 1 }, { "mod5Dest", 1 }, { "mod5Amt", 0.2f },
            { "cutoff", 2500 }, { "verbMix", 0.4f }, { "verbSize", 0.75f }, { "verbShimmer", 0.15f } }), mLead },
        { "Melodic Techno Bass", "Techno & Euro", with (bassBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "ampD", 0.5f }, { "ampS", 0.5f },
            { "cutoff", 400 }, { "fltEnv", 0.2f }, { "modD", 0.25f }, { "subOn", 1 }, { "subOct", -1 }, { "subLevel", 0.4f } }), mBass },
        { "Italo Arp", "Techno & Euro", with (stabBase, { { "chord", 0 }, { "arpOn", 1 }, { "arpMode", 2 }, { "arpRate", 3 }, { "arpOct", 2 }, { "arpGate", 0.6f },
            { "aTable", 2 }, { "aPos", 0.2f }, { "ampD", 0.2f }, { "ampS", 0.1f }, { "cutoff", 2500 }, { "fltEnv", 0.2f },
            { "chorusMix", 0.35f }, { "dlyTime", 5 }, { "dlyMix", 0.15f } }), mStab },
        { "Hard Groove Organ", "Techno & Euro", with (stabBase, { { "chord", 0 }, { "aTable", 9 }, { "aPos", 0.2f }, { "ampD", 0.15f }, { "ampS", 0 },
            { "fltType", 3 }, { "cutoff", 1500 }, { "res", 0.3f }, { "distType", 0 }, { "distDrive", 0.5f }, { "distMix", 0.6f } }), mStab },
        { "Warehouse Stab", "Techno & Euro", with (stabBase, { { "chord", 5 }, { "aTable", 7 }, { "aPos", 0.3f }, { "ampD", 0.3f },
            { "distType", 0 }, { "distDrive", 0.5f }, { "distMix", 0.6f }, { "verbMix", 0.35f }, { "verbSize", 0.75f } }), mStab },
        { "Rave Piano", "Techno & Euro", with (stabBase, { { "chord", 5 }, { "aTable", 12 }, { "aPos", 0.6f }, { "aWarp", 6 }, { "aWarpAmt", 0.08f },
            { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0 }, { "bOct", 1 }, { "bLevel", 0.001f }, { "bFilter", 0 },
            { "ampD", 1.0f }, { "ampS", 0 }, { "modD", 0.3f }, { "mod5Src", 3 }, { "mod5Dest", 3 }, { "mod5Amt", 0.45f },
            { "fltType", 0 }, { "cutoff", 7000 }, { "fltEnv", 0 }, { "verbMix", 0.25f } }), mStab },

        //================================================================ Dub & Dancehall
        { "Dub Siren Classic", "Dub & Dancehall", with (sirenBase, {}), mSiren },
        { "Siren Rise", "Dub & Dancehall", with (sirenBase, { { "dropAmt", -24 }, { "dropTime", 1.0f }, { "lfo1Rate", 7.0f }, { "mod1Amt", 0.12f } }), mSiren },
        { "Siren Fall", "Dub & Dancehall", with (sirenBase, { { "dropAmt", 24 }, { "dropTime", 1.0f }, { "lfo1Rate", 6.0f }, { "mod1Amt", 0.1f } }), mSiren },
        { "Siren Wobble Down", "Dub & Dancehall", with (sirenBase, { { "lfo1Shape", 2 }, { "lfo1Rate", 2.0f }, { "mod1Amt", 0.8f } }), mSiren },
        { "Two Tone Siren", "Dub & Dancehall", with (sirenBase, { { "lfo1Shape", 4 }, { "lfo1Rate", 3.5f }, { "mod1Amt", 0.25f }, { "verbShimmer", 0.2f } }), mSiren },
        { "Space Echo Siren", "Dub & Dancehall", with (sirenBase, { { "aTable", 0 }, { "aPos", 0 }, { "lfo1Shape", 0 }, { "lfo1Rate", 5.0f },
            { "mod1Amt", 0.35f }, { "dlyFb", 0.82f }, { "dlyMix", 0.55f }, { "verbShimmer", 0.35f }, { "verbMix", 0.35f } }), mSiren },
        { "Lazer Zap", "Dub & Dancehall", with (sirenBase, { { "aTable", 0 }, { "aPos", 0.05f }, { "mode", 0 }, { "dropAmt", 36 }, { "dropTime", 0.15f },
            { "ampD", 0.25f }, { "ampS", 0 }, { "mod1Amt", 0 }, { "dlyFb", 0.5f }, { "dlyMix", 0.3f } }), mSiren },
        { "Pew Pew", "Dub & Dancehall", with (sirenBase, { { "mode", 0 }, { "dropAmt", 48 }, { "dropTime", 0.08f }, { "ampD", 0.15f }, { "ampS", 0 },
            { "mod1Amt", 0 }, { "dlyTime", 4 }, { "dlyFb", 0.55f }, { "dlyMix", 0.35f } }), mSiren },
        { "Sleng Teng Bass", "Dub & Dancehall", with (bassBase, { { "aTable", 2 }, { "aPos", 0.12f }, { "ampD", 0.45f }, { "ampS", 0.55f },
            { "fltType", 0 }, { "cutoff", 1400 }, { "res", 0.1f }, { "lfo2Rate", 5.0f }, { "mod5Src", 2 }, { "mod5Dest", 7 }, { "mod5Amt", 0.004f },
            { "distType", 5 }, { "distDrive", 0.15f }, { "distMix", 0.3f } }), mBass },
        { "Digital Dancehall Bass", "Dub & Dancehall", with (bassBase, { { "aTable", 12 }, { "aPos", 0.2f }, { "aWarp", 6 }, { "aWarpAmt", 0.03f },
            { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0 }, { "bOct", 1 }, { "bLevel", 0.001f }, { "bFilter", 0 },
            { "ampD", 0.5f }, { "ampS", 0.3f }, { "fltOn", 0 }, { "modD", 0.08f }, { "modS", 0 },
            { "mod1Src", 3 }, { "mod1Dest", 3 }, { "mod1Amt", 0.4f }, { "subOn", 1 }, { "subOct", 0 }, { "subLevel", 0.3f } }), mBass },
        { "Steppers Dub Bass", "Dub & Dancehall", with (bassBase, { { "aTable", 0 }, { "aPos", 0.03f }, { "glide", 0.03f },
            { "ampD", 0.9f }, { "ampS", 0.7f }, { "fltType", 0 }, { "cutoff", 400 },
            { "distType", 0 }, { "distDrive", 0.2f }, { "distMix", 0.4f } }), mBass },
        { "Roots Bass", "Dub & Dancehall", with (bassBase, { { "aTable", 0 }, { "aPos", 0.33f }, { "ampD", 0.6f }, { "ampS", 0.6f },
            { "fltType", 0 }, { "cutoff", 300 }, { "fltEnv", 0.15f }, { "modD", 0.1f }, { "modS", 0 } }), mBass },
        { "Dancehall FM Lead", "Dub & Dancehall", with (leadBase, { { "mode", 0 }, { "aTable", 12 }, { "aPos", 0.2f }, { "aWarp", 6 }, { "aWarpAmt", 0.08f },
            { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0 }, { "bOct", 1 }, { "bLevel", 0.001f }, { "bFilter", 0 },
            { "ampD", 0.6f }, { "ampS", 0.3f }, { "fltOn", 0 }, { "modD", 0.25f }, { "modS", 0 },
            { "mod1Src", 3 }, { "mod1Dest", 3 }, { "mod1Amt", 0.35f } }), mLead },
        { "Dembow Pluck", "Dub & Dancehall", with (stabBase, { { "chord", 0 }, { "aTable", 7 }, { "aPos", 0.15f }, { "ampD", 0.18f }, { "ampS", 0 },
            { "cutoff", 2500 }, { "fltEnv", 0.2f }, { "dlyTime", 3 }, { "dlyMix", 0.15f } }), mStab },
        { "Air Horn", "Dub & Dancehall", with (leadBase, { { "mode", 0 }, { "chord", 3 }, { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 5 }, { "aDetune", 0.2f },
            { "dropAmt", -3 }, { "dropTime", 0.12f }, { "ampS", 0.9f }, { "ampR", 0.3f }, { "cutoff", 3000 },
            { "lfo2Rate", 7.0f }, { "lfo2Fade", 0.3f }, { "mod2Amt", 0.02f }, { "distType", 0 }, { "distDrive", 0.3f }, { "distMix", 0.4f },
            { "dlyTime", 5 }, { "dlyFb", 0.5f }, { "dlyMix", 0.3f } }), mLead },
        { "Dub Chord Skank", "Dub & Dancehall", with (stabBase, { { "chord", 5 }, { "aTable", 9 }, { "aPos", 0.2f }, { "ampD", 0.12f }, { "ampS", 0 },
            { "fltType", 2 }, { "cutoff", 300 }, { "fltEnv", 0 }, { "dlyTime", 5 }, { "dlyFb", 0.55f }, { "dlyTone", 0.4f }, { "dlyMix", 0.35f },
            { "verbMix", 0.2f } }), mStab },
        { "Melodica Lead", "Dub & Dancehall", with (leadBase, { { "aTable", 2 }, { "aPos", 0.3f }, { "noiseLevel", 0.05f }, { "noiseTone", 0.9f },
            { "fltType", 1 }, { "cutoff", 2500 }, { "lfo2Fade", 0.6f }, { "mod2Amt", 0.015f }, { "dlyTime", 5 }, { "dlyFb", 0.5f }, { "dlyMix", 0.3f } }), mLead },
        { "Echo Drop FX", "Dub & Dancehall", with (sirenBase, { { "aOn", 0 }, { "noiseLevel", 0.8f }, { "noiseTone", 0.8f }, { "fltType", 3 }, { "cutoff", 1500 },
            { "res", 0.6f }, { "mod1Dest", 8 }, { "mod1Amt", 0.5f }, { "lfo1Rate", 0.5f }, { "dlyFb", 0.78f } }), mSiren },
        { "Bashment Bass", "Dub & Dancehall", with (base808, { { "aPos", 0.3f }, { "mode", 1 }, { "glide", 0 }, { "dropAmt", 12 }, { "dropTime", 0.02f },
            { "ampD", 0.6f }, { "ampS", 0.2f }, { "distType", 0 }, { "distDrive", 0.35f }, { "distMix", 0.8f } }), m808 },

        //================================================================ Lead
        { "Dean Solo", "Lead", with (leadBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 3 }, { "aDetune", 0.08f },
            { "bOn", 1 }, { "bTable", 1 }, { "bPos", 0.3f }, { "bLevel", 0.4f }, { "glide", 0.12f }, { "bendRange", 2 },
            { "cutoff", 2200 }, { "res", 0.25f }, { "fltEnv", 0.3f }, { "modD", 0.6f }, { "modS", 0.3f },
            { "distType", 0 }, { "distDrive", 0.3f }, { "distMix", 0.5f }, { "dlyTime", 8 }, { "dlyFb", 0.45f }, { "dlyMix", 0.3f },
            { "verbSize", 0.7f }, { "verbMix", 0.35f } }), mLead },
        { "Moog Portamento", "Lead", with (leadBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "bOn", 1 }, { "bTable", 2 }, { "bPos", 0 }, { "bOct", -1 },
            { "bLevel", 0.45f }, { "glide", 0.15f }, { "cutoff", 3000 }, { "res", 0.2f }, { "fltEnv", 0.2f }, { "modD", 0.5f }, { "modS", 0.4f },
            { "distType", 0 }, { "distDrive", 0.25f }, { "distMix", 0.4f }, { "dlyTime", 7 } }), mLead },
        { "Astro Lead", "Lead", with (leadBase, { { "aTable", 11 }, { "aPos", 0.4f }, { "aUni", 3 }, { "aDetune", 0.12f },
            { "cutoff", 5000 }, { "modD", 0.5f }, { "modS", 0.2f }, { "mod5Src", 3 }, { "mod5Dest", 1 }, { "mod5Amt", 0.4f },
            { "dlyFb", 0.4f }, { "dlyMix", 0.25f }, { "verbSize", 0.7f }, { "verbMix", 0.3f }, { "verbShimmer", 0.2f } }), mLead },
        { "Sirens", "Lead", with (leadBase, { { "aTable", 0 }, { "aPos", 0.1f }, { "aUni", 2 }, { "aDetune", 0.05f }, { "glide", 0.2f },
            { "ampA", 0.02f }, { "ampR", 0.6f }, { "fltOn", 0 }, { "lfo1Shape", 1 }, { "lfo1Rate", 0.35f }, { "lfo1Retrig", 0 },
            { "mod5Src", 1 }, { "mod5Dest", 7 }, { "mod5Amt", 0.25f }, { "dlyTime", 8 }, { "dlyFb", 0.5f }, { "dlyMix", 0.3f },
            { "verbSize", 0.85f }, { "verbMix", 0.45f }, { "verbShimmer", 0.25f } }), mLead },
        { "Rager Pluck", "Lead", with (stabBase, { { "chord", 0 }, { "aTable", 2 }, { "aPos", 0.2f }, { "aUni", 2 }, { "aDetune", 0.08f },
            { "bOn", 1 }, { "bTable", 9 }, { "bPos", 0.3f }, { "bOct", 1 }, { "bLevel", 0.35f }, { "ampD", 0.3f }, { "ampS", 0 }, { "ampR", 0.3f },
            { "cutoff", 700 }, { "fltEnv", 0.5f }, { "modD", 0.2f }, { "dlyTime", 4 }, { "dlyFb", 0.3f }, { "dlyMix", 0.2f },
            { "verbSize", 0.75f }, { "verbMix", 0.3f } }), mStab },
        { "Cactus Bell", "Lead", with (leadBase, { { "mode", 0 }, { "aTable", 10 }, { "aPos", 0.5f }, { "ampD", 1.2f }, { "ampS", 0 }, { "ampR", 0.8f },
            { "fltOn", 0 }, { "dlyTime", 7 }, { "dlyFb", 0.35f }, { "dlyMix", 0.2f }, { "verbSize", 0.7f }, { "verbMix", 0.35f }, { "verbShimmer", 0.3f } }), mLead },
        { "Stargaze Bell", "Lead", with (leadBase, { { "mode", 0 }, { "aTable", 12 }, { "aPos", 0.2f }, { "aWarp", 6 }, { "aWarpAmt", 0.1f },
            { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0 }, { "bOct", 2 }, { "bSemi", 7 }, { "bLevel", 0.001f }, { "bFilter", 0 },
            { "ampD", 2.5f }, { "ampS", 0 }, { "ampR", 2.0f }, { "fltOn", 0 }, { "modD", 0.6f }, { "modS", 0 },
            { "mod5Src", 3 }, { "mod5Dest", 3 }, { "mod5Amt", 0.35f }, { "dlyTime", 8 }, { "dlyFb", 0.4f }, { "dlyMix", 0.2f },
            { "verbSize", 0.85f }, { "verbMix", 0.4f }, { "verbShimmer", 0.45f } }), mLead },
        { "G-Funk Whistle", "Lead", with (leadBase, { { "aTable", 0 }, { "aPos", 0.0f }, { "glide", 0.12f }, { "fltOn", 0 },
            { "lfo2Rate", 5.5f }, { "lfo2Fade", 0.4f }, { "mod2Amt", 0.03f }, { "verbMix", 0.2f } }), mLead },
        { "Trap Flute", "Lead", with (leadBase, { { "aTable", 6 }, { "aPos", 0.85f }, { "noiseLevel", 0.08f }, { "noiseTone", 0.8f },
            { "ampA", 0.03f }, { "fltType", 0 }, { "cutoff", 2800 }, { "lfo2Fade", 0.3f }, { "mod2Amt", 0.02f } }), mLead },
        { "Dean Sync Lead", "Lead", with (leadBase, { { "aTable", 1 }, { "aPos", 0.35f }, { "aUni", 2 }, { "aDetune", 0.06f },
            { "glide", 0.1f }, { "modD", 0.8f }, { "modS", 0.2f }, { "mod5Src", 3 }, { "mod5Dest", 1 }, { "mod5Amt", 0.35f },
            { "cutoff", 4000 }, { "distType", 0 }, { "distDrive", 0.35f }, { "distMix", 0.5f }, { "dlyTime", 8 }, { "dlyFb", 0.45f }, { "dlyMix", 0.3f } }), mLead },
        { "Saw Anthem Lead", "Lead", with (leadBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.3f },
            { "cutoff", 5000 }, { "verbMix", 0.3f }, { "width", 1.2f } }), mLead },
        { "Chip Lead", "Lead", with (leadBase, { { "aTable", 7 }, { "aPos", 0.8f }, { "glide", 0.02f }, { "fltOn", 0 },
            { "lfo2Rate", 7.0f }, { "lfo2Fade", 0.2f }, { "mod2Amt", 0.02f } }), mLead },
        { "Portamento Pulse", "Lead", with (leadBase, { { "aTable", 2 }, { "aPos", 0.3f }, { "aWarp", 4 }, { "aWarpAmt", 0.2f },
            { "lfo1Rate", 0.4f }, { "lfo1Retrig", 0 }, { "mod5Src", 1 }, { "mod5Dest", 3 }, { "mod5Amt", 0.3f }, { "glide", 0.15f }, { "cutoff", 3000 } }), mLead },
        { "Detroit Lead", "Lead", with (leadBase, { { "aTable", 2 }, { "aPos", 0.1f }, { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0.66f }, { "bSemi", 7 },
            { "bLevel", 0.35f }, { "cutoff", 2500 }, { "dlyTime", 5 }, { "dlyFb", 0.5f }, { "dlyMix", 0.28f } }), mLead },

        //================================================================ Soundscape
        { "Hypernova", "Soundscape", with (spaceBase, { { "aTable", 14 }, { "aPos", 0.5f }, { "aUni", 7 }, { "aDetune", 0.25f }, { "aBlend", 0.8f },
            { "bOn", 1 }, { "bTable", 13 }, { "bPos", 0.4f }, { "bOct", 1 }, { "bLevel", 0.35f }, { "verbShimmer", 0.5f }, { "verbMix", 0.6f } }), mSpace },
        { "Event Horizon", "Soundscape", with (spaceBase, { { "aTable", 6 }, { "aPos", 0.1f }, { "aOct", -1 }, { "aUni", 5 }, { "aDetune", 0.2f },
            { "bOn", 1 }, { "bTable", 10 }, { "bPos", 0.2f }, { "bLevel", 0.2f }, { "fltType", 1 }, { "cutoff", 800 }, { "res", 0.3f },
            { "lfo1Rate", 0.03f }, { "mod1Amt", 0.4f }, { "mod2Dest", 2 }, { "verbShimmer", 0.2f },
            { "dlyTime", 9 }, { "dlyFb", 0.5f }, { "dlyMix", 0.2f } }), mSpace },
        { "Aurora Glass", "Soundscape", with (spaceBase, { { "aTable", 13 }, { "aPos", 0.3f }, { "aUni", 3 }, { "aDetune", 0.1f },
            { "ampA", 1.2f }, { "cutoff", 6000 }, { "verbShimmer", 0.7f }, { "verbSize", 0.85f }, { "verbMix", 0.5f } }), mSpace },
        { "Orbit Choir", "Soundscape", with (spaceBase, { { "aTable", 6 }, { "aPos", 0.7f }, { "aUni", 7 }, { "aDetune", 0.2f }, { "aBlend", 0.7f },
            { "cutoff", 3500 }, { "verbShimmer", 0.3f }, { "verbSize", 0.8f } }), mSpace },
        { "Stellar Drift", "Soundscape", with (spaceBase, { { "aTable", 9 }, { "aPos", 0.4f }, { "aUni", 5 }, { "aDetune", 0.15f },
            { "lfo2Shape", 1 }, { "lfo2Rate", 0.05f }, { "mod2Amt", 0.5f }, { "cutoff", 4000 },
            { "verbSize", 1.0f }, { "verbMix", 0.5f }, { "dlyTime", 8 }, { "dlyFb", 0.5f }, { "dlyMix", 0.25f } }), mSpace },
        { "Deep Space Pad", "Soundscape", with (spaceBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.3f }, { "aBlend", 0.8f },
            { "bOn", 1 }, { "bTable", 14 }, { "bPos", 0.6f }, { "bOct", 1 }, { "bLevel", 0.3f }, { "cutoff", 1200 }, { "mod2Dest", 2 } }), mSpace },
        { "Nebula Keys", "Soundscape", with (spaceBase, { { "aTable", 12 }, { "aPos", 0.2f }, { "aWarp", 6 }, { "aWarpAmt", 0.08f },
            { "bOn", 1 }, { "bTable", 13 }, { "bPos", 0.3f }, { "bOct", 1 }, { "bLevel", 0.002f }, { "bFilter", 0 },
            { "ampA", 0.005f }, { "ampD", 3.0f }, { "ampS", 0 }, { "ampR", 3.0f }, { "cutoff", 5000 }, { "mod2Dest", 3 }, { "mod2Amt", 0.15f },
            { "verbShimmer", 0.6f }, { "verbSize", 0.8f }, { "verbMix", 0.45f } }), mSpace },
        { "Solar Wind", "Soundscape", with (spaceBase, { { "aTable", 14 }, { "aPos", 0.2f }, { "aLevel", 0.3f }, { "noiseLevel", 0.6f }, { "noiseTone", 0.85f },
            { "fltType", 3 }, { "cutoff", 1200 }, { "res", 0.6f }, { "lfo1Rate", 0.06f }, { "mod1Amt", 0.8f },
            { "verbSize", 0.95f }, { "verbMix", 0.6f }, { "verbShimmer", 0.25f } }), mSpace },
        { "Cryo Sleep", "Soundscape", with (spaceBase, { { "aTable", 13 }, { "aPos", 0.6f }, { "aUni", 5 }, { "aDetune", 0.12f },
            { "bOn", 1 }, { "bTable", 14 }, { "bPos", 0.8f }, { "bOct", 1 }, { "bLevel", 0.25f },
            { "ampA", 4.0f }, { "ampR", 7.0f }, { "cutoff", 5000 }, { "verbShimmer", 0.8f }, { "verbSize", 0.9f }, { "verbMix", 0.6f } }), mSpace },
        { "Gravity Well", "Soundscape", with (spaceBase, { { "aTable", 14 }, { "aPos", 0.3f }, { "aOct", -1 }, { "aUni", 5 }, { "aDetune", 0.2f },
            { "subOn", 1 }, { "subOct", -1 }, { "subLevel", 0.45f }, { "cutoff", 900 }, { "lfo1Rate", 0.04f }, { "mod1Amt", 0.45f },
            { "verbShimmer", 0.15f }, { "verbMix", 0.45f } }), mSpace },
        { "Utopia Pad", "Soundscape", with (spaceBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.3f }, { "aBlend", 0.8f },
            { "bOn", 1 }, { "bTable", 9 }, { "bPos", 0.6f }, { "bOct", 1 }, { "bLevel", 0.3f },
            { "ampA", 0.8f }, { "ampD", 2.0f }, { "ampR", 2.5f }, { "cutoff", 1800 }, { "verbShimmer", 0.2f } }), mSpace },
        { "Dark Choir", "Soundscape", with (spaceBase, { { "aTable", 6 }, { "aPos", 0.6f }, { "aUni", 5 }, { "aDetune", 0.15f },
            { "ampA", 0.3f }, { "ampS", 0.9f }, { "ampR", 1.5f }, { "cutoff", 2500 }, { "verbShimmer", 0.1f } }), mSpace },
        { "Singularity", "Soundscape", with (spaceBase, { { "aTable", 5 }, { "aPos", 0.15f }, { "aOct", -1 }, { "aUni", 7 }, { "aDetune", 0.35f },
            { "bOn", 1 }, { "bTable", 13 }, { "bPos", 0.7f }, { "bOct", 2 }, { "bLevel", 0.15f },
            { "fltType", 1 }, { "cutoff", 700 }, { "res", 0.35f }, { "lfo1Rate", 0.02f }, { "mod1Amt", 0.6f },
            { "lfo2Rate", 0.09f }, { "mod2Amt", 0.35f }, { "dlyTime", 10 }, { "dlyFb", 0.55f }, { "dlyMix", 0.2f },
            { "verbShimmer", 0.45f }, { "verbSize", 1.0f } }), mSpace },
        { "Andromeda", "Soundscape", with (spaceBase, { { "aTable", 9 }, { "aPos", 0.7f }, { "aUni", 7 }, { "aDetune", 0.2f },
            { "bOn", 1 }, { "bTable", 14 }, { "bPos", 0.3f }, { "bOct", -1 }, { "bLevel", 0.4f }, { "cutoff", 3000 },
            { "verbShimmer", 0.55f }, { "verbMix", 0.55f } }), mSpace },
        { "Cosmic Rain", "Soundscape", with (spaceBase, { { "arpOn", 1 }, { "arpMode", 3 }, { "arpRate", 3 }, { "arpOct", 3 }, { "arpGate", 0.3f },
            { "aTable", 13 }, { "aPos", 0.4f }, { "ampA", 0.002f }, { "ampD", 0.6f }, { "ampS", 0 }, { "ampR", 0.8f }, { "cutoff", 7000 },
            { "dlyTime", 5 }, { "dlyFb", 0.5f }, { "dlyMix", 0.3f }, { "verbShimmer", 0.5f } }), mSpace },
        { "Hyperspace", "Soundscape", with (spaceBase, { { "aTable", 14 }, { "aPos", 0.9f }, { "aUni", 7 }, { "aDetune", 0.4f },
            { "noiseLevel", 0.2f }, { "noiseTone", 0.9f }, { "fltType", 3 }, { "cutoff", 2000 }, { "res", 0.4f },
            { "lfo1Shape", 3 }, { "lfo1Rate", 0.1f }, { "mod1Amt", 0.7f }, { "verbShimmer", 0.6f } }), mSpace },
        { "Ocean of Stars", "Soundscape", with (spaceBase, { { "aTable", 13 }, { "aPos", 0.8f }, { "aUni", 5 }, { "aDetune", 0.15f },
            { "bOn", 1 }, { "bTable", 6 }, { "bPos", 0.7f }, { "bLevel", 0.3f }, { "lfo2Rate", 0.08f }, { "mod2Amt", 0.4f },
            { "chorusMix", 0.5f }, { "verbShimmer", 0.45f } }), mSpace },
        { "Lunar Base", "Soundscape", with (spaceBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 3 }, { "aDetune", 0.1f },
            { "fltType", 6 }, { "cutoff", 220 }, { "res", 0.8f }, { "lfo1Rate", 0.05f }, { "mod1Amt", 0.5f }, { "verbShimmer", 0.15f } }), mSpace },
        { "Deep Field", "Soundscape", with (spaceBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.25f },
            { "fltType", 7 }, { "cutoff", 250 }, { "res", 0.55f }, { "lfo1Rate", 0.04f }, { "mod1Amt", 0.8f } }), mSpace },
        { "Afterglow", "Soundscape", with (spaceBase, { { "aTable", 12 }, { "aPos", 0.6f }, { "aUni", 5 }, { "aDetune", 0.15f },
            { "bOn", 1 }, { "bTable", 9 }, { "bPos", 0.3f }, { "bOct", 1 }, { "bLevel", 0.3f }, { "cutoff", 2000 }, { "verbShimmer", 0.3f } }), mSpace },
        { "Zero Gravity", "Soundscape", with (spaceBase, { { "chord", 11 }, { "aTable", 14 }, { "aPos", 0.4f }, { "aUni", 3 }, { "aDetune", 0.15f },
            { "strum", 0.06f }, { "cutoff", 3500 }, { "verbShimmer", 0.5f } }), mSpace },
        { "Star Map", "Soundscape", with (spaceBase, { { "arpOn", 1 }, { "arpMode", 2 }, { "arpRate", 1 }, { "arpOct", 2 }, { "arpGate", 0.8f },
            { "aTable", 9 }, { "aPos", 0.5f }, { "ampA", 0.05f }, { "ampD", 1.0f }, { "ampS", 0.3f }, { "ampR", 1.5f },
            { "dlyTime", 8 }, { "dlyFb", 0.5f }, { "dlyMix", 0.25f } }), mSpace },

        //================================================================ FX
        { "Riser Up", "FX", with (spaceBase, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.3f }, { "noiseLevel", 0.35f },
            { "dropAmt", -36 }, { "dropTime", 1.0f }, { "ampA", 2.0f }, { "cutoff", 1200 }, { "fltEnv", 0.5f }, { "modA", 3.0f }, { "modS", 1.0f },
            { "verbShimmer", 0.2f } }), mSpace },
        { "Downlifter", "FX", with (spaceBase, { { "aTable", 14 }, { "aPos", 0.6f }, { "aUni", 5 }, { "dropAmt", 24 }, { "dropTime", 1.0f },
            { "ampA", 0.01f }, { "ampD", 3.0f }, { "ampS", 0 }, { "noiseLevel", 0.3f } }), mSpace },
        { "Impact Boom", "FX", with (base808, { { "aTable", 0 }, { "aPos", 0 }, { "mode", 0 }, { "dropAmt", 36 }, { "dropTime", 0.35f },
            { "ampD", 3.0f }, { "ampS", 0 }, { "noiseLevel", 0.2f }, { "distType", 0 }, { "distDrive", 0.4f }, { "distMix", 0.7f },
            { "verbSize", 0.9f }, { "verbMix", 0.4f } }), m808 },
        { "White Noise Sweep", "FX", with (spaceBase, { { "aOn", 0 }, { "noiseLevel", 0.9f }, { "noiseTone", 1.0f }, { "ampA", 0.5f },
            { "fltType", 3 }, { "cutoff", 600 }, { "res", 0.5f }, { "lfo1Shape", 3 }, { "lfo1Sync", 2 }, { "lfo1Retrig", 1 }, { "mod1Amt", 0.8f } }), mSpace },
        { "Laser Rain", "FX", with (spaceBase, { { "arpOn", 1 }, { "arpMode", 3 }, { "arpRate", 5 }, { "arpOct", 3 }, { "arpGate", 0.3f },
            { "aTable", 0 }, { "aPos", 0.05f }, { "dropAmt", 24 }, { "dropTime", 0.06f }, { "ampA", 0.001f }, { "ampD", 0.12f }, { "ampS", 0 },
            { "ampR", 0.1f }, { "dlyTime", 3 }, { "dlyFb", 0.5f }, { "dlyMix", 0.3f } }), mSpace },
        { "Alarm", "FX", with (sirenBase, { { "aTable", 2 }, { "lfo1Shape", 4 }, { "lfo1Rate", 4.0f }, { "mod1Amt", 0.1f }, { "dlyMix", 0.2f } }), mSiren },
        { "Glitch Stutter", "FX", with (wobBase, { { "aTable", 7 }, { "aPos", 0.5f }, { "lfo1Shape", 5 }, { "lfo1Sync", 9 },
            { "mod1Dest", 7 }, { "mod1Amt", 0.3f }, { "mod2Amt", 0.5f }, { "mod3Dest", 16 }, { "mod3Amt", -0.5f } }), mWob },
        { "Sci-Fi Beeps", "FX", with (stabBase, { { "chord", 0 }, { "arpOn", 1 }, { "arpMode", 3 }, { "arpRate", 3 }, { "arpOct", 3 }, { "arpGate", 0.25f },
            { "aTable", 0 }, { "aPos", 0.0f }, { "ampD", 0.1f }, { "ampS", 0 }, { "fltOn", 0 }, { "dlyTime", 5 }, { "dlyFb", 0.4f }, { "dlyMix", 0.25f } }), mStab },
        { "Transmission", "FX", with (spaceBase, { { "aTable", 7 }, { "aPos", 0.8f }, { "aWarp", 5 }, { "aWarpAmt", 0.5f }, { "noiseLevel", 0.25f },
            { "fltType", 3 }, { "cutoff", 1800 }, { "res", 0.5f }, { "lfo1Shape", 5 }, { "lfo1Sync", 8 }, { "lfo1Retrig", 1 }, { "mod1Amt", 0.5f } }), mSpace },
        { "Wormhole", "FX", with (spaceBase, { { "aTable", 5 }, { "aPos", 0.5f }, { "aUni", 7 }, { "aDetune", 0.5f },
            { "lfo1Shape", 3 }, { "lfo1Rate", 0.25f }, { "mod1Dest", 7 }, { "mod1Amt", 0.5f }, { "verbShimmer", 0.7f } }), mSpace },
    };
    static const std::vector<Preset> presets = []
    {
        auto v = basePresets;
        auto more = morePresets();
        v.insert (v.end(), more.begin(), more.end());
        return v;
    }();
    return presets;
}

} // namespace ab
