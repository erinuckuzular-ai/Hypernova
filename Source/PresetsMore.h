#pragma once

// Everything that isn't bass: keys, plucks, pads, strings and brass, bells, voices, arps, synth drums,
// cinematic, retro and world. Appended after the original bank so existing program numbers never move.
// Same index cheat sheet as Presets.h.
namespace ab
{

inline std::vector<Preset> morePresets()
{
    // FM building block: Osc A = carrier (Rich Sine), Osc B = near-silent modulator whose octave/semi set the
    // ratio, mod env drives the FM index. Classic DX-style keys and bells.
    static const PresetValues fm
    {
        { "aTable", 12 }, { "aPos", 0.15f }, { "aWarp", 6 }, { "aWarpAmt", 0.04f },
        { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0 }, { "bLevel", 0.001f }, { "bFilter", 0 },
        { "mode", 0 }, { "velSens", 0.7f }, { "fltOn", 0 },
        { "ampA", 0.002f }, { "ampD", 2.0f }, { "ampS", 0.0f }, { "ampR", 0.6f },
        { "modA", 0.001f }, { "modD", 0.5f }, { "modS", 0 }, { "modR", 0.4f },
        { "mod1Src", 3 }, { "mod1Dest", 3 }, { "mod1Amt", 0.25f },
        { "mod2Src", 4 }, { "mod2Dest", 3 }, { "mod2Amt", 0.1f },
        { "mod3Src", 7 }, { "mod3Dest", 3 }, { "mod3Amt", 0.3f },
        { "mod4Src", 10 }, { "mod4Dest", 23 }, { "mod4Amt", 0.5f },
        { "verbSize", 0.5f }, { "verbMix", 0.15f },
    };
    static const Macros mFm { "TINE", "MACRO 2", "MACRO 3", "SPACE" };

    // Subtractive poly: saw/pulse through a filter with its own envelope.
    static const PresetValues poly
    {
        { "mode", 0 }, { "velSens", 0.6f }, { "retrig", 0 },
        { "ampA", 0.003f }, { "ampD", 0.8f }, { "ampS", 0.6f }, { "ampR", 0.4f },
        { "fltType", 1 }, { "cutoff", 1800 }, { "res", 0.15f }, { "fltEnv", 0.3f }, { "fltKey", 0.4f },
        { "modA", 0.002f }, { "modD", 0.4f }, { "modS", 0.2f }, { "modR", 0.4f },
        { "mod1Src", 7 }, { "mod1Dest", 8 }, { "mod1Amt", 0.6f },
        { "mod2Src", 4 }, { "mod2Dest", 8 }, { "mod2Amt", 0.25f },
        { "mod3Src", 9 }, { "mod3Dest", 22 }, { "mod3Amt", 0.5f },
        { "mod4Src", 10 }, { "mod4Dest", 23 }, { "mod4Amt", 0.5f },
        { "verbSize", 0.55f }, { "verbMix", 0.18f },
    };
    static const Macros mPoly { "BRIGHT", "MACRO 2", "ECHO", "SPACE" };

    // Pads: slow, wide, moving.
    static const PresetValues pad
    {
        { "mode", 0 }, { "retrig", 0 }, { "velSens", 0.3f }, { "drift", 0.25f },
        { "ampA", 0.9f }, { "ampD", 3.0f }, { "ampS", 0.85f }, { "ampR", 2.5f },
        { "fltType", 0 }, { "cutoff", 2600 }, { "res", 0.12f },
        { "lfo1Rate", 0.08f }, { "lfo1Retrig", 0 }, { "lfo2Shape", 6 }, { "lfo2Rate", 0.2f }, { "lfo2Retrig", 0 },
        { "mod1Src", 1 }, { "mod1Dest", 8 }, { "mod1Amt", 0.2f },
        { "mod2Src", 2 }, { "mod2Dest", 1 }, { "mod2Amt", 0.2f },
        { "mod3Src", 7 }, { "mod3Dest", 8 }, { "mod3Amt", 0.6f },
        { "mod4Src", 10 }, { "mod4Dest", 23 }, { "mod4Amt", 0.4f },
        { "chorusMix", 0.35f }, { "verbSize", 0.8f }, { "verbMix", 0.38f }, { "width", 1.2f },
    };
    static const Macros mPad { "BLOOM", "MACRO 2", "MACRO 3", "SPACE" };

    // Synth drums: one-shots, no sustain, velocity matters.
    static const PresetValues drum
    {
        { "mode", 0 }, { "retrig", 1 }, { "velSens", 0.85f }, { "monoBass", 1 },
        { "ampA", 0.001f }, { "ampS", 0 },
        { "mod1Src", 7 }, { "mod1Dest", 19 }, { "mod1Amt", 0.6f },
        { "mod2Src", 8 }, { "mod2Dest", 23 }, { "mod2Amt", 0.5f },
    };
    static const Macros mDrum { "DIRT", "ROOM", "MACRO 3", "MACRO 4" };

    // Plucked string: a noise burst (gated by the mod env) rings a comb filter tuned to the note.
    static const PresetValues string
    {
        { "mode", 0 }, { "retrig", 1 }, { "velSens", 0.7f },
        { "aOn", 0 }, { "noiseLevel", 0.0f }, { "noiseTone", 0.9f }, { "noiseFilter", 1 },
        { "fltType", 6 }, { "cutoff", 261.6f }, { "fltKey", 1.0f }, { "res", 0.985f },
        { "ampA", 0.001f }, { "ampD", 2.0f }, { "ampS", 0 }, { "ampR", 0.4f },
        { "modA", 0.001f }, { "modD", 0.03f }, { "modS", 0 },
        { "mod1Src", 3 }, { "mod1Dest", 12 }, { "mod1Amt", 1.0f },
        { "mod2Src", 7 }, { "mod2Dest", 9 }, { "mod2Amt", 0.05f },
        { "verbSize", 0.45f }, { "verbMix", 0.15f },
    };
    static const Macros mString { "SUSTAIN", "MACRO 2", "MACRO 3", "MACRO 4" };

    auto P = [] (const char* name, const char* cat, PresetValues v, Macros m) { return Preset { name, cat, std::move (v), m }; };

    return {
        //================================================================ Keys
        P ("FM Rhodes", "Keys", with (fm, { { "chorusMix", 0.25f }, { "lfo1Rate", 4.5f }, { "lfo1Retrig", 0 },
            { "mod5Src", 1 }, { "mod5Dest", 15 }, { "mod5Amt", 0.25f } }), mFm),
        P ("Suitcase EP", "Keys", with (fm, { { "aPos", 0.2f }, { "mod1Amt", 0.32f }, { "lfo1Rate", 4.0f }, { "lfo1Retrig", 0 },
            { "mod5Src", 1 }, { "mod5Dest", 16 }, { "mod5Amt", -0.25f }, { "distType", 0 }, { "distDrive", 0.2f }, { "distMix", 0.3f } }), mFm),
        P ("Wurli Bite", "Keys", with (fm, { { "aPos", 0.5f }, { "mod1Amt", 0.4f }, { "modD", 0.25f }, { "ampD", 1.4f },
            { "distType", 0 }, { "distDrive", 0.35f }, { "distMix", 0.5f } }), mFm),
        P ("Dream EP", "Keys", with (fm, { { "chorusMix", 0.45f }, { "verbMix", 0.35f }, { "verbShimmer", 0.25f }, { "dlyTime", 5 }, { "dlyMix", 0.15f } }), mFm),
        P ("DX Piano", "Keys", with (fm, { { "bOct", 2 }, { "aWarpAmt", 0.02f }, { "mod1Amt", 0.35f }, { "modD", 0.3f }, { "ampD", 2.5f } }), mFm),
        P ("Bright House Piano", "Keys", with (fm, { { "aPos", 0.6f }, { "aWarpAmt", 0.08f }, { "mod1Amt", 0.45f }, { "modD", 0.35f }, { "ampD", 1.2f },
            { "verbMix", 0.2f }, { "ott", 0.25f } }), mFm),
        P ("Upright Keys", "Keys", with (fm, { { "aTable", 9 }, { "aPos", 0.15f }, { "mod1Amt", 0.2f }, { "ampD", 1.8f }, { "fltOn", 1 }, { "fltType", 0 }, { "cutoff", 3500 } }), mFm),
        P ("Drawbar Organ", "Keys", with (poly, { { "aTable", 9 }, { "aPos", 0.0f }, { "bOn", 1 }, { "bTable", 9 }, { "bPos", 0.25f }, { "bOct", 1 }, { "bLevel", 0.5f },
            { "ampA", 0.004f }, { "ampS", 1.0f }, { "ampR", 0.08f }, { "fltType", 0 }, { "cutoff", 6000 }, { "fltEnv", 0 },
            { "lfo1Rate", 6.5f }, { "lfo1Retrig", 0 }, { "mod5Src", 1 }, { "mod5Dest", 15 }, { "mod5Amt", 0.35f }, { "chorusMix", 0.35f } }), mPoly),
        P ("Church Organ", "Keys", with (poly, { { "aTable", 9 }, { "aPos", 0.45f }, { "aUni", 3 }, { "aDetune", 0.05f },
            { "bOn", 1 }, { "bTable", 9 }, { "bPos", 0.1f }, { "bOct", -1 }, { "bLevel", 0.5f },
            { "ampA", 0.08f }, { "ampS", 1.0f }, { "ampR", 1.2f }, { "fltType", 0 }, { "cutoff", 5000 }, { "fltEnv", 0 },
            { "verbSize", 0.95f }, { "verbMix", 0.5f } }), mPoly),
        P ("Clavinet", "Keys", with (poly, { { "aTable", 2 }, { "aPos", 0.35f }, { "ampD", 0.5f }, { "ampS", 0.1f }, { "ampR", 0.05f },
            { "fltType", 3 }, { "cutoff", 1400 }, { "res", 0.3f }, { "fltEnv", 0.45f }, { "modD", 0.12f }, { "modS", 0 },
            { "mod2Amt", 0.4f }, { "distType", 0 }, { "distDrive", 0.25f }, { "distMix", 0.4f } }), mPoly),
        P ("Harpsichord", "Keys", with (poly, { { "aTable", 2 }, { "aPos", 0.45f }, { "bOn", 1 }, { "bTable", 1 }, { "bPos", 0.4f }, { "bOct", 1 }, { "bLevel", 0.3f },
            { "ampD", 1.2f }, { "ampS", 0 }, { "ampR", 0.3f }, { "fltType", 2 }, { "cutoff", 400 }, { "fltEnv", 0 } }), mPoly),
        P ("Celeste", "Keys", with (fm, { { "bOct", 2 }, { "aPos", 0.05f }, { "mod1Amt", 0.18f }, { "ampD", 1.6f }, { "verbMix", 0.25f } }), mFm),
        P ("Toy Piano", "Keys", with (fm, { { "aTable", 10 }, { "aPos", 0.2f }, { "bOct", 2 }, { "bSemi", 3 }, { "mod1Amt", 0.25f }, { "ampD", 0.9f } }), mFm),
        P ("Vapor Keys", "Keys", with (fm, { { "chorusMix", 0.5f }, { "drift", 0.45f }, { "distType", 5 }, { "distDrive", 0.2f }, { "distMix", 0.35f },
            { "fltOn", 1 }, { "fltType", 0 }, { "cutoff", 2400 }, { "verbMix", 0.3f } }), mFm),

        //================================================================ Plucks
        P ("Future Pluck", "Plucks", with (poly, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 5 }, { "aDetune", 0.22f }, { "ampD", 0.35f }, { "ampS", 0 },
            { "cutoff", 700 }, { "fltEnv", 0.55f }, { "modD", 0.22f }, { "modS", 0 }, { "dlyTime", 5 }, { "dlyFb", 0.35f }, { "dlyMix", 0.18f } }), mPoly),
        P ("Glass Pluck", "Plucks", with (poly, { { "aTable", 13 }, { "aPos", 0.35f }, { "ampD", 0.6f }, { "ampS", 0 }, { "fltType", 0 }, { "cutoff", 5000 }, { "fltEnv", 0 },
            { "verbShimmer", 0.3f }, { "verbMix", 0.3f } }), mPoly),
        P ("Harp Pluck", "Plucks", with (fm, { { "aPos", 0.3f }, { "mod1Amt", 0.12f }, { "ampD", 1.8f }, { "fltOn", 1 }, { "fltType", 2 }, { "cutoff", 180 } }), mFm),
        P ("Tropical Pluck", "Plucks", with (poly, { { "aTable", 0 }, { "aPos", 0.2f }, { "ampD", 0.22f }, { "ampS", 0 }, { "fltType", 0 }, { "cutoff", 3000 }, { "fltEnv", 0.2f },
            { "dlyTime", 5 }, { "dlyFb", 0.4f }, { "dlyMix", 0.25f } }), mPoly),
        P ("Plucked String", "Plucks", with (string, {}), mString),
        P ("Nylon String", "Plucks", with (string, { { "res", 0.975f }, { "noiseTone", 0.6f }, { "ampD", 1.4f } }), mString),
        P ("Steel String", "Plucks", with (string, { { "res", 0.992f }, { "noiseTone", 1.0f }, { "ampD", 3.0f }, { "chorusMix", 0.15f } }), mString),
        P ("Muted Guitar", "Plucks", with (string, { { "res", 0.93f }, { "ampD", 0.25f }, { "ampR", 0.08f }, { "noiseTone", 0.5f } }), mString),
        P ("Water Drop", "Plucks", with (poly, { { "aTable", 0 }, { "aPos", 0 }, { "dropAmt", -14 }, { "dropTime", 0.06f }, { "ampD", 0.25f }, { "ampS", 0 },
            { "fltOn", 0 }, { "verbMix", 0.3f }, { "dlyTime", 3 }, { "dlyMix", 0.2f } }), mPoly),
        P ("Bubble Pluck", "Plucks", with (poly, { { "aTable", 6 }, { "aPos", 0.9f }, { "ampD", 0.2f }, { "ampS", 0 }, { "fltType", 3 }, { "cutoff", 400 }, { "res", 0.6f },
            { "fltEnv", 0.6f }, { "modD", 0.1f }, { "modS", 0 } }), mPoly),
        P ("Pizzicato", "Plucks", with (poly, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 3 }, { "aDetune", 0.1f }, { "ampD", 0.3f }, { "ampS", 0 },
            { "cutoff", 1200 }, { "fltEnv", 0.3f }, { "modD", 0.15f }, { "modS", 0 }, { "verbMix", 0.3f } }), mPoly),
        P ("Mallet Pluck", "Plucks", with (fm, { { "bOct", 2 }, { "mod1Amt", 0.3f }, { "modD", 0.08f }, { "ampD", 0.5f } }), mFm),

        //================================================================ Pads
        P ("Warm Analog Pad", "Pads", with (pad, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 5 }, { "aDetune", 0.2f },
            { "bOn", 1 }, { "bTable", 2 }, { "bPos", 0.2f }, { "bLevel", 0.35f }, { "cutoff", 1600 } }), mPad),
        P ("Glass Pad", "Pads", with (pad, { { "aTable", 13 }, { "aPos", 0.5f }, { "aUni", 3 }, { "aDetune", 0.1f }, { "cutoff", 6000 }, { "verbShimmer", 0.35f } }), mPad),
        P ("Choir Pad", "Pads", with (pad, { { "aTable", 6 }, { "aPos", 0.2f }, { "aUni", 5 }, { "aDetune", 0.15f }, { "cutoff", 3500 } }), mPad),
        P ("Evolving Air", "Pads", with (pad, { { "aTable", 14 }, { "aPos", 0.3f }, { "aUni", 7 }, { "aDetune", 0.2f }, { "lfo2Rate", 0.05f }, { "mod2Amt", 0.5f } }), mPad),
        P ("Dark Pad", "Pads", with (pad, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 5 }, { "aDetune", 0.15f }, { "aOct", -1 },
            { "fltType", 1 }, { "cutoff", 600 }, { "res", 0.25f } }), mPad),
        P ("Sweep Pad", "Pads", with (pad, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.25f }, { "fltType", 1 }, { "cutoff", 500 }, { "res", 0.35f },
            { "lfo1Shape", 1 }, { "lfo1Sync", 2 }, { "mod1Amt", 0.6f } }), mPad),
        P ("Detroit Pad", "Pads", with (pad, { { "aTable", 2 }, { "aPos", 0.15f }, { "aUni", 3 }, { "aDetune", 0.12f }, { "cutoff", 2000 },
            { "dlyTime", 5 }, { "dlyFb", 0.45f }, { "dlyMix", 0.2f } }), mPad),
        P ("Lo-Fi Tape Pad", "Pads", with (pad, { { "aTable", 0 }, { "aPos", 0.4f }, { "aUni", 3 }, { "drift", 0.7f }, { "cutoff", 1800 },
            { "distType", 5 }, { "distDrive", 0.25f }, { "distMix", 0.35f }, { "chorusMix", 0.5f } }), mPad),
        P ("Ambient Swell", "Pads", with (pad, { { "aTable", 9 }, { "aPos", 0.5f }, { "aUni", 5 }, { "ampA", 2.5f }, { "ampR", 4.0f }, { "verbMix", 0.55f }, { "verbShimmer", 0.3f } }), mPad),
        P ("String Pad", "Pads", with (pad, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.18f }, { "ampA", 0.4f }, { "cutoff", 3200 },
            { "lfo2Shape", 0 }, { "lfo2Rate", 5.0f }, { "lfo2Fade", 1.0f }, { "mod2Dest", 7 }, { "mod2Amt", 0.008f } }), mPad),
        P ("Motion Pad", "Pads", with (pad, { { "aTable", 5 }, { "aPos", 0.2f }, { "aUni", 5 }, { "lfo1Shape", 0 }, { "lfo1Sync", 4 }, { "mod1Dest", 1 }, { "mod1Amt", 0.35f } }), mPad),
        P ("Ethereal Pad", "Pads", with (pad, { { "aTable", 13 }, { "aPos", 0.7f }, { "aUni", 5 }, { "bOn", 1 }, { "bTable", 14 }, { "bPos", 0.5f }, { "bOct", 1 }, { "bLevel", 0.3f },
            { "cutoff", 7000 }, { "verbShimmer", 0.6f }, { "verbMix", 0.5f } }), mPad),

        //================================================================ Strings & Brass
        P ("Synth Strings", "Strings & Brass", with (poly, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.18f }, { "ampA", 0.25f }, { "ampS", 0.9f },
            { "ampR", 0.8f }, { "fltType", 0 }, { "cutoff", 4000 }, { "fltEnv", 0 }, { "lfo2Rate", 5.2f }, { "lfo2Fade", 0.6f },
            { "mod5Src", 2 }, { "mod5Dest", 7 }, { "mod5Amt", 0.008f }, { "chorusMix", 0.4f }, { "verbMix", 0.3f } }), mPoly),
        P ("Ensemble Strings", "Strings & Brass", with (poly, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.28f }, { "bOn", 1 }, { "bTable", 0 },
            { "bPos", 0.66f }, { "bOct", 1 }, { "bUni", 5 }, { "bLevel", 0.3f }, { "ampA", 0.35f }, { "ampS", 0.9f }, { "ampR", 1.0f },
            { "fltType", 0 }, { "cutoff", 3000 }, { "fltEnv", 0 }, { "chorusMix", 0.6f }, { "verbMix", 0.35f } }), mPoly),
        P ("Cello Solo", "Strings & Brass", with (poly, { { "mode", 2 }, { "glide", 0.06f }, { "aTable", 0 }, { "aPos", 0.66f }, { "ampA", 0.12f }, { "ampS", 0.9f },
            { "fltType", 3 }, { "cutoff", 900 }, { "res", 0.2f }, { "fltEnv", 0 }, { "lfo2Rate", 5.0f }, { "lfo2Fade", 0.5f },
            { "mod5Src", 2 }, { "mod5Dest", 7 }, { "mod5Amt", 0.012f }, { "verbMix", 0.3f } }), mPoly),
        P ("Brass Stab", "Strings & Brass", with (poly, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 3 }, { "aDetune", 0.1f }, { "ampA", 0.02f }, { "ampD", 0.5f }, { "ampS", 0.6f },
            { "cutoff", 900 }, { "fltEnv", 0.45f }, { "modA", 0.05f }, { "modD", 0.4f }, { "modS", 0.4f } }), mPoly),
        P ("Horn Section", "Strings & Brass", with (poly, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 5 }, { "aDetune", 0.12f }, { "ampA", 0.06f }, { "ampS", 0.85f },
            { "cutoff", 1200 }, { "fltEnv", 0.35f }, { "modA", 0.08f }, { "modD", 0.6f }, { "modS", 0.5f }, { "chorusMix", 0.3f } }), mPoly),
        P ("Trumpet Lead", "Strings & Brass", with (poly, { { "mode", 2 }, { "glide", 0.04f }, { "aTable", 0 }, { "aPos", 0.66f }, { "ampA", 0.03f }, { "ampS", 0.9f },
            { "fltType", 3 }, { "cutoff", 1400 }, { "res", 0.3f }, { "fltEnv", 0.3f }, { "modA", 0.04f }, { "modD", 0.3f }, { "modS", 0.5f },
            { "lfo2Rate", 5.5f }, { "lfo2Fade", 0.4f }, { "mod5Src", 2 }, { "mod5Dest", 7 }, { "mod5Amt", 0.012f } }), mPoly),
        P ("Sax Lead", "Strings & Brass", with (poly, { { "mode", 2 }, { "glide", 0.05f }, { "aTable", 6 }, { "aPos", 0.3f }, { "noiseLevel", 0.06f },
            { "ampA", 0.03f }, { "ampS", 0.9f }, { "fltType", 7 }, { "cutoff", 400 }, { "res", 0.4f }, { "fltEnv", 0.25f }, { "modD", 0.4f },
            { "lfo2Rate", 5.0f }, { "lfo2Fade", 0.5f }, { "mod5Src", 2 }, { "mod5Dest", 7 }, { "mod5Amt", 0.015f } }), mPoly),
        P ("Flute", "Strings & Brass", with (poly, { { "mode", 2 }, { "glide", 0.04f }, { "aTable", 0 }, { "aPos", 0.05f }, { "noiseLevel", 0.12f }, { "noiseTone", 0.8f },
            { "ampA", 0.06f }, { "ampS", 0.9f }, { "fltType", 0 }, { "cutoff", 4000 }, { "fltEnv", 0 },
            { "lfo2Rate", 5.0f }, { "lfo2Fade", 0.6f }, { "mod5Src", 2 }, { "mod5Dest", 7 }, { "mod5Amt", 0.01f } }), mPoly),
        P ("Oboe", "Strings & Brass", with (poly, { { "mode", 2 }, { "aTable", 2 }, { "aPos", 0.7f }, { "ampA", 0.04f }, { "ampS", 0.9f },
            { "fltType", 3 }, { "cutoff", 1200 }, { "res", 0.35f }, { "fltEnv", 0 }, { "lfo2Rate", 5.2f }, { "lfo2Fade", 0.5f },
            { "mod5Src", 2 }, { "mod5Dest", 7 }, { "mod5Amt", 0.01f } }), mPoly),
        P ("Epic Strings", "Strings & Brass", with (poly, { { "chord", 3 }, { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.2f }, { "ampA", 0.5f },
            { "ampS", 0.9f }, { "ampR", 1.5f }, { "fltType", 0 }, { "cutoff", 3500 }, { "fltEnv", 0 }, { "verbSize", 0.85f }, { "verbMix", 0.4f } }), mPoly),

        //================================================================ Bells & Mallets
        P ("Tubular Bell", "Bells & Mallets", with (fm, { { "bOct", 1 }, { "bSemi", 6 }, { "mod1Amt", 0.35f }, { "modD", 1.2f }, { "ampD", 5.0f }, { "ampR", 3.0f } }), mFm),
        P ("Music Box", "Bells & Mallets", with (fm, { { "bOct", 2 }, { "mod1Amt", 0.15f }, { "modD", 0.2f }, { "ampD", 1.8f }, { "verbMix", 0.3f } }), mFm),
        P ("Glockenspiel", "Bells & Mallets", with (fm, { { "aOct", 1 }, { "bOct", 3 }, { "bSemi", 5 }, { "mod1Amt", 0.25f }, { "modD", 0.15f }, { "ampD", 2.2f } }), mFm),
        P ("Vibraphone", "Bells & Mallets", with (fm, { { "bOct", 2 }, { "mod1Amt", 0.2f }, { "modD", 0.2f }, { "ampD", 3.0f }, { "lfo1Rate", 5.5f }, { "lfo1Retrig", 0 },
            { "mod5Src", 1 }, { "mod5Dest", 16 }, { "mod5Amt", -0.3f } }), mFm),
        P ("Marimba", "Bells & Mallets", with (fm, { { "bOct", 2 }, { "mod1Amt", 0.45f }, { "modD", 0.04f }, { "ampD", 0.6f }, { "ampR", 0.2f } }), mFm),
        P ("Crystal Bell", "Bells & Mallets", with (fm, { { "aTable", 13 }, { "aPos", 0.4f }, { "bOct", 1 }, { "bSemi", 9 }, { "mod1Amt", 0.3f }, { "ampD", 3.0f },
            { "verbShimmer", 0.4f }, { "verbMix", 0.3f } }), mFm),
        P ("Temple Bell", "Bells & Mallets", with (fm, { { "aTable", 10 }, { "aPos", 0.3f }, { "aOct", -1 }, { "bOct", 1 }, { "bSemi", 2 }, { "mod1Amt", 0.4f },
            { "modD", 2.0f }, { "ampD", 7.0f }, { "ampR", 4.0f }, { "verbMix", 0.35f } }), mFm),
        P ("Chime", "Bells & Mallets", with (fm, { { "aTable", 13 }, { "aPos", 0.7f }, { "aOct", 1 }, { "mod1Amt", 0.1f }, { "ampD", 2.5f } }), mFm),
        P ("Steel Drum", "Bells & Mallets", with (fm, { { "bSemi", 7 }, { "bOct", 1 }, { "mod1Amt", 0.3f }, { "modD", 0.12f }, { "ampD", 0.9f } }), mFm),
        P ("Kalimba", "Bells & Mallets", with (fm, { { "bOct", 3 }, { "mod1Amt", 0.35f }, { "modD", 0.03f }, { "ampD", 0.9f }, { "ampR", 0.3f } }), mFm),

        //================================================================ Vocal & Choir
        P ("Ahh Choir", "Vocal & Choir", with (pad, { { "aTable", 6 }, { "aPos", 0.0f }, { "aUni", 7 }, { "aDetune", 0.15f }, { "ampA", 0.35f }, { "cutoff", 4000 } }), mPad),
        P ("Ooh Choir", "Vocal & Choir", with (pad, { { "aTable", 6 }, { "aPos", 0.95f }, { "aUni", 7 }, { "aDetune", 0.15f }, { "ampA", 0.35f }, { "cutoff", 2500 } }), mPad),
        P ("Formant Lead", "Vocal & Choir", with (poly, { { "mode", 2 }, { "glide", 0.06f }, { "aTable", 0 }, { "aPos", 0.66f }, { "ampS", 0.9f },
            { "fltType", 7 }, { "cutoff", 300 }, { "res", 0.55f }, { "fltEnv", 0 }, { "lfo1Rate", 0.6f }, { "lfo1Retrig", 0 },
            { "mod5Src", 1 }, { "mod5Dest", 8 }, { "mod5Amt", 0.5f } }), mPoly),
        P ("Talkbox Lead", "Vocal & Choir", with (poly, { { "mode", 2 }, { "glide", 0.05f }, { "aTable", 0 }, { "aPos", 0.66f }, { "ampS", 0.9f },
            { "fltType", 7 }, { "cutoff", 250 }, { "res", 0.6f }, { "fltEnv", 0.6f }, { "modA", 0.05f }, { "modD", 0.3f }, { "modS", 0.3f } }), mPoly),
        P ("Robot Voice", "Vocal & Choir", with (poly, { { "aTable", 6 }, { "aPos", 0.5f }, { "aWarp", 5 }, { "aWarpAmt", 0.4f }, { "ampS", 0.9f },
            { "lfo1Shape", 5 }, { "lfo1Sync", 8 }, { "mod5Src", 1 }, { "mod5Dest", 1 }, { "mod5Amt", 0.4f } }), mPoly),
        P ("Whisper Pad", "Vocal & Choir", with (pad, { { "aOn", 0 }, { "noiseLevel", 0.7f }, { "noiseTone", 0.85f }, { "fltType", 7 }, { "cutoff", 400 }, { "res", 0.7f },
            { "lfo1Rate", 0.15f }, { "mod1Amt", 0.6f } }), mPad),
        P ("Angel Choir", "Vocal & Choir", with (pad, { { "aTable", 6 }, { "aPos", 0.3f }, { "aUni", 7 }, { "aOct", 1 }, { "verbShimmer", 0.6f }, { "verbMix", 0.5f } }), mPad),
        P ("Vocoder Chords", "Vocal & Choir", with (poly, { { "chord", 9 }, { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 3 }, { "ampA", 0.05f }, { "ampS", 0.8f },
            { "fltType", 7 }, { "cutoff", 350 }, { "res", 0.5f }, { "fltEnv", 0 }, { "lfo1Sync", 5 }, { "lfo1Retrig", 0 },
            { "mod5Src", 1 }, { "mod5Dest", 8 }, { "mod5Amt", 0.4f } }), mPoly),

        //================================================================ Arps
        P ("Up Arp Pluck", "Arps", with (poly, { { "arpOn", 1 }, { "arpMode", 0 }, { "arpRate", 3 }, { "arpOct", 2 }, { "arpGate", 0.5f },
            { "aTable", 0 }, { "aPos", 0.66f }, { "ampD", 0.2f }, { "ampS", 0 }, { "cutoff", 1000 }, { "fltEnv", 0.45f }, { "modD", 0.15f }, { "modS", 0 },
            { "dlyTime", 5 }, { "dlyMix", 0.15f } }), mPoly),
        P ("Bouncing Arp", "Arps", with (poly, { { "arpOn", 1 }, { "arpMode", 1 }, { "arpRate", 4 }, { "arpOct", 2 }, { "arpGate", 0.4f },
            { "aTable", 2 }, { "aPos", 0.3f }, { "ampD", 0.18f }, { "ampS", 0 }, { "cutoff", 1500 }, { "fltEnv", 0.35f }, { "modD", 0.1f } }), mPoly),
        P ("Random Glass Arp", "Arps", with (poly, { { "arpOn", 1 }, { "arpMode", 3 }, { "arpRate", 3 }, { "arpOct", 3 }, { "arpGate", 0.35f },
            { "aTable", 13 }, { "aPos", 0.4f }, { "ampD", 0.4f }, { "ampS", 0 }, { "fltType", 0 }, { "cutoff", 6000 }, { "fltEnv", 0 }, { "verbMix", 0.3f } }), mPoly),
        P ("Chord Arp", "Arps", with (poly, { { "arpOn", 1 }, { "arpMode", 2 }, { "arpRate", 3 }, { "arpOct", 1 }, { "chord", 9 },
            { "aTable", 0 }, { "aPos", 0.66f }, { "ampD", 0.25f }, { "ampS", 0 }, { "cutoff", 1400 }, { "fltEnv", 0.3f } }), mPoly),
        P ("80s Sequence", "Arps", with (poly, { { "arpOn", 1 }, { "arpMode", 0 }, { "arpRate", 3 }, { "arpOct", 1 }, { "arpGate", 0.7f },
            { "aTable", 2 }, { "aPos", 0.2f }, { "ampD", 0.3f }, { "ampS", 0.3f }, { "fltType", 1 }, { "cutoff", 800 }, { "res", 0.35f }, { "fltEnv", 0.4f },
            { "modD", 0.15f }, { "chorusMix", 0.35f }, { "dlyTime", 5 }, { "dlyMix", 0.2f } }), mPoly),
        P ("Stranger Arp", "Arps", with (poly, { { "arpOn", 1 }, { "arpMode", 2 }, { "arpRate", 3 }, { "arpOct", 3 }, { "arpGate", 0.6f },
            { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 2 }, { "ampD", 0.3f }, { "ampS", 0.2f }, { "fltType", 1 }, { "cutoff", 600 }, { "res", 0.4f },
            { "fltEnv", 0.45f }, { "modD", 0.18f }, { "verbMix", 0.3f } }), mPoly),
        P ("Minimal Arp", "Arps", with (poly, { { "arpOn", 1 }, { "arpMode", 4 }, { "arpRate", 3 }, { "arpOct", 1 }, { "arpGate", 0.25f },
            { "aTable", 0 }, { "aPos", 0.0f }, { "ampD", 0.12f }, { "ampS", 0 }, { "fltOn", 0 }, { "dlyTime", 2 }, { "dlyFb", 0.45f }, { "dlyMix", 0.25f } }), mPoly),
        P ("Afro Marimba Arp", "Arps", with (fm, { { "arpOn", 1 }, { "arpMode", 2 }, { "arpRate", 3 }, { "arpOct", 2 }, { "arpGate", 0.5f },
            { "bOct", 2 }, { "mod1Amt", 0.45f }, { "modD", 0.04f }, { "ampD", 0.5f } }), mFm),
        P ("Octave Pulse Arp", "Arps", with (poly, { { "arpOn", 1 }, { "arpMode", 0 }, { "arpRate", 3 }, { "arpOct", 2 }, { "arpGate", 0.5f },
            { "aTable", 2 }, { "aPos", 0.5f }, { "ampD", 0.2f }, { "ampS", 0.1f }, { "cutoff", 2000 }, { "fltEnv", 0.2f } }), mPoly),
        P ("Trance Arp", "Arps", with (poly, { { "arpOn", 1 }, { "arpMode", 0 }, { "arpRate", 3 }, { "arpOct", 3 }, { "arpGate", 0.6f },
            { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.3f }, { "ampD", 0.25f }, { "ampS", 0.1f },
            { "cutoff", 2000 }, { "fltEnv", 0.3f }, { "dlyTime", 5 }, { "dlyFb", 0.4f }, { "dlyMix", 0.25f }, { "verbMix", 0.3f } }), mPoly),

        //================================================================ Synth drums
        P ("909 Kick", "Synth Drums", with (drum, { { "aTable", 0 }, { "aPos", 0.05f }, { "dropAmt", 36 }, { "dropTime", 0.08f }, { "ampD", 0.45f }, { "fltOn", 0 },
            { "noiseLevel", 0.25f }, { "noiseFilter", 0 }, { "modD", 0.01f }, { "modS", 0 }, { "mod3Src", 3 }, { "mod3Dest", 12 }, { "mod3Amt", 0.4f },
            { "distType", 0 }, { "distDrive", 0.25f }, { "distMix", 0.5f } }), mDrum),
        P ("Deep Kick", "Synth Drums", with (drum, { { "aTable", 0 }, { "aPos", 0 }, { "dropAmt", 30 }, { "dropTime", 0.12f }, { "ampD", 0.9f }, { "fltOn", 0 } }), mDrum),
        P ("Hard Techno Kick", "Synth Drums", with (drum, { { "aTable", 0 }, { "aPos", 0.1f }, { "dropAmt", 40 }, { "dropTime", 0.07f }, { "ampD", 0.6f }, { "fltOn", 0 },
            { "distType", 1 }, { "distDrive", 0.6f }, { "distMix", 1 }, { "ott", 0.3f } }), mDrum),
        P ("808 Kick", "Synth Drums", with (drum, { { "aTable", 3 }, { "aPos", 0.1f }, { "dropAmt", 24 }, { "dropTime", 0.05f }, { "ampD", 1.2f }, { "fltOn", 0 } }), mDrum),
        P ("Tone Snare", "Synth Drums", with (drum, { { "aTable", 0 }, { "aPos", 0.3f }, { "aOct", 1 }, { "dropAmt", 7 }, { "dropTime", 0.03f }, { "aLevel", 0.5f },
            { "noiseLevel", 0.8f }, { "noiseTone", 0.85f }, { "noiseFilter", 1 }, { "ampD", 0.3f }, { "fltType", 2 }, { "cutoff", 200 } }), mDrum),
        P ("Noise Snare", "Synth Drums", with (drum, { { "aOn", 0 }, { "noiseLevel", 1.0f }, { "noiseTone", 0.9f }, { "ampD", 0.25f },
            { "fltType", 3 }, { "cutoff", 2500 }, { "res", 0.2f }, { "fltKey", 0 } }), mDrum),
        P ("Hand Clap", "Synth Drums", with (drum, { { "aOn", 0 }, { "noiseLevel", 1.0f }, { "noiseTone", 0.8f }, { "ampD", 0.35f },
            { "fltType", 3 }, { "cutoff", 1400 }, { "res", 0.3f }, { "lfo1Shape", 2 }, { "lfo1Rate", 28.0f }, { "lfo1Retrig", 1 }, { "lfo1Fade", 0.0f },
            { "mod3Src", 1 }, { "mod3Dest", 16 }, { "mod3Amt", 0.8f }, { "verbMix", 0.15f }, { "verbSize", 0.35f } }), mDrum),
        P ("Closed Hat", "Synth Drums", with (drum, { { "aOn", 0 }, { "noiseLevel", 1.0f }, { "noiseTone", 1.0f }, { "ampD", 0.07f }, { "ampR", 0.04f },
            { "fltType", 2 }, { "cutoff", 7000 }, { "fltKey", 0 } }), mDrum),
        P ("Open Hat", "Synth Drums", with (drum, { { "aOn", 0 }, { "noiseLevel", 1.0f }, { "noiseTone", 1.0f }, { "ampD", 0.5f }, { "ampR", 0.2f },
            { "fltType", 2 }, { "cutoff", 6500 }, { "fltKey", 0 } }), mDrum),
        P ("Synth Tom", "Synth Drums", with (drum, { { "aTable", 0 }, { "aPos", 0.05f }, { "dropAmt", 12 }, { "dropTime", 0.12f }, { "ampD", 0.6f }, { "fltOn", 0 } }), mDrum),
        P ("Rimshot", "Synth Drums", with (drum, { { "aTable", 10 }, { "aPos", 0.5f }, { "aOct", 1 }, { "ampD", 0.06f }, { "fltType", 3 }, { "cutoff", 1800 }, { "res", 0.4f } }), mDrum),
        P ("Cowbell", "Synth Drums", with (drum, { { "aTable", 2 }, { "aPos", 0.0f }, { "bOn", 1 }, { "bTable", 2 }, { "bPos", 0 }, { "bSemi", 7 }, { "bLevel", 0.6f },
            { "ampD", 0.3f }, { "fltType", 3 }, { "cutoff", 900 }, { "res", 0.3f }, { "fltKey", 1.0f } }), mDrum),
        P ("Shaker", "Synth Drums", with (drum, { { "aOn", 0 }, { "noiseLevel", 1.0f }, { "noiseTone", 1.0f }, { "ampA", 0.02f }, { "ampD", 0.12f },
            { "fltType", 2 }, { "cutoff", 5000 }, { "fltKey", 0 } }), mDrum),
        P ("Laser Perc", "Synth Drums", with (drum, { { "aTable", 2 }, { "aPos", 0.2f }, { "dropAmt", 36 }, { "dropTime", 0.05f }, { "ampD", 0.15f }, { "fltOn", 0 } }), mDrum),

        //================================================================ Cinematic
        P ("Braam", "Cinematic", with (pad, { { "aTable", 0 }, { "aPos", 0.66f }, { "aOct", -1 }, { "aUni", 7 }, { "aDetune", 0.25f },
            { "bOn", 1 }, { "bTable", 2 }, { "bPos", 0.2f }, { "bOct", -1 }, { "bLevel", 0.5f }, { "ampA", 0.05f }, { "ampD", 3.0f }, { "ampS", 0.5f },
            { "fltType", 1 }, { "cutoff", 700 }, { "fltEnv", 0.4f }, { "modA", 0.5f }, { "modD", 2.0f }, { "modS", 0.3f },
            { "distType", 0 }, { "distDrive", 0.5f }, { "distMix", 0.7f }, { "verbSize", 0.9f }, { "verbMix", 0.4f } }), mPad),
        P ("Dark Drone", "Cinematic", with (pad, { { "aTable", 6 }, { "aPos", 0.9f }, { "aOct", -1 }, { "aUni", 5 }, { "fltType", 1 }, { "cutoff", 1400 }, { "res", 0.3f },
            { "lfo1Rate", 0.03f }, { "mod1Amt", 0.5f }, { "verbShimmer", 0.1f } }), mPad),
        P ("Impact Hit", "Cinematic", with (drum, { { "aTable", 0 }, { "aPos", 0 }, { "aOct", -1 }, { "dropAmt", 24 }, { "dropTime", 0.3f }, { "ampD", 3.0f },
            { "noiseLevel", 0.4f }, { "noiseFilter", 0 }, { "fltOn", 0 }, { "distType", 0 }, { "distDrive", 0.5f }, { "distMix", 0.7f },
            { "verbSize", 0.95f }, { "verbMix", 0.45f } }), mDrum),
        P ("Sub Drop", "Cinematic", with (drum, { { "aTable", 0 }, { "aPos", 0 }, { "dropAmt", 24 }, { "dropTime", 1.0f }, { "ampD", 4.0f }, { "fltOn", 0 } }), mDrum),
        P ("Metallic Scrape", "Cinematic", with (pad, { { "aTable", 10 }, { "aPos", 0.8f }, { "fltType", 6 }, { "cutoff", 300 }, { "res", 0.85f }, { "fltKey", 0 },
            { "lfo1Shape", 6 }, { "lfo1Rate", 0.3f }, { "mod1Amt", 0.7f } }), mPad),
        P ("Horror Swell", "Cinematic", with (pad, { { "chord", 16 }, { "aTable", 6 }, { "aPos", 0.8f }, { "aUni", 5 }, { "ampA", 3.0f }, { "cutoff", 1500 },
            { "lfo2Shape", 0 }, { "lfo2Rate", 6.0f }, { "lfo2Fade", 2.0f }, { "mod2Dest", 7 }, { "mod2Amt", 0.03f } }), mPad),
        P ("Epic Pad", "Cinematic", with (pad, { { "chord", 5 }, { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.2f },
            { "bOn", 1 }, { "bTable", 6 }, { "bPos", 0.2f }, { "bLevel", 0.35f }, { "cutoff", 3000 }, { "verbSize", 1.0f }, { "verbMix", 0.5f } }), mPad),
        P ("Tension Pulse", "Cinematic", with (poly, { { "aTable", 0 }, { "aPos", 0.66f }, { "aOct", -1 }, { "ampS", 0.9f }, { "cutoff", 500 }, { "fltEnv", 0 },
            { "lfo1Shape", 4 }, { "lfo1Sync", 7 }, { "lfo1Retrig", 0 }, { "mod5Src", 1 }, { "mod5Dest", 16 }, { "mod5Amt", -0.7f },
            { "dlyTime", 7 }, { "dlyMix", 0.2f } }), mPoly),

        //================================================================ Retro & Chip
        P ("Chip Square Lead", "Retro & Chip", with (poly, { { "mode", 1 }, { "aTable", 7 }, { "aPos", 0.95f }, { "aWarp", 4 }, { "aWarpAmt", 0.0f }, { "ampS", 0.9f }, { "fltOn", 0 },
            { "lfo2Rate", 6.0f }, { "lfo2Fade", 0.3f }, { "mod5Src", 2 }, { "mod5Dest", 7 }, { "mod5Amt", 0.02f } }), mPoly),
        P ("Chip Arp", "Retro & Chip", with (poly, { { "arpOn", 1 }, { "arpMode", 0 }, { "arpRate", 5 }, { "arpOct", 1 }, { "chord", 4 },
            { "aTable", 2 }, { "aPos", 0.0f }, { "ampS", 0.9f }, { "fltOn", 0 }, { "distType", 5 }, { "distDrive", 0.35f }, { "distMix", 0.6f } }), mPoly),
        P ("8-Bit Pluck", "Retro & Chip", with (poly, { { "aTable", 2 }, { "aPos", 0.6f }, { "ampD", 0.2f }, { "ampS", 0 }, { "fltOn", 0 },
            { "distType", 5 }, { "distDrive", 0.5f }, { "distMix", 0.7f } }), mPoly),
        P ("NES Triangle", "Retro & Chip", with (poly, { { "mode", 1 }, { "aTable", 7 }, { "aPos", 0.6f }, { "ampS", 1.0f }, { "fltOn", 0 } }), mPoly),
        P ("Game Over", "Retro & Chip", with (poly, { { "aTable", 2 }, { "aPos", 0.0f }, { "dropAmt", -24 }, { "dropTime", 0.8f }, { "ampD", 1.0f }, { "ampS", 0 }, { "fltOn", 0 },
            { "distType", 5 }, { "distDrive", 0.4f }, { "distMix", 0.6f } }), mPoly),
        P ("Retro Laser", "Retro & Chip", with (poly, { { "aTable", 2 }, { "aPos", 0.3f }, { "dropAmt", 30 }, { "dropTime", 0.2f }, { "ampD", 0.3f }, { "ampS", 0 }, { "fltOn", 0 },
            { "dlyTime", 3 }, { "dlyMix", 0.25f } }), mPoly),
        P ("Power Up", "Retro & Chip", with (poly, { { "arpOn", 1 }, { "arpMode", 0 }, { "arpRate", 5 }, { "arpOct", 3 }, { "chord", 4 },
            { "aTable", 2 }, { "aPos", 0.2f }, { "ampS", 0.8f }, { "fltOn", 0 }, { "distType", 5 }, { "distDrive", 0.3f }, { "distMix", 0.5f } }), mPoly),
        P ("Coin", "Retro & Chip", with (poly, { { "aTable", 2 }, { "aPos", 0.0f }, { "aOct", 1 }, { "dropAmt", -5 }, { "dropTime", 0.06f }, { "ampD", 0.35f }, { "ampS", 0 },
            { "fltOn", 0 } }), mPoly),

        //================================================================ World
        P ("Koto", "World", with (string, { { "res", 0.985f }, { "noiseTone", 0.95f }, { "ampD", 1.6f }, { "bendRange", 2 }, { "verbMix", 0.25f } }), mString),
        P ("Sitar", "World", with (string, { { "aOn", 1 }, { "aTable", 1 }, { "aPos", 0.3f }, { "aLevel", 0.12f }, { "res", 0.99f }, { "ampD", 3.0f },
            { "chorusMix", 0.2f }, { "verbMix", 0.2f } }), mString),
        P ("Pan Flute", "World", with (poly, { { "mode", 2 }, { "aTable", 0 }, { "aPos", 0.12f }, { "noiseLevel", 0.25f }, { "noiseTone", 0.7f },
            { "ampA", 0.05f }, { "ampS", 0.85f }, { "fltType", 0 }, { "cutoff", 3000 }, { "fltEnv", 0 }, { "verbMix", 0.3f } }), mPoly),
        P ("Balafon", "World", with (fm, { { "bOct", 2 }, { "bSemi", 1 }, { "mod1Amt", 0.5f }, { "modD", 0.03f }, { "ampD", 0.45f }, { "distType", 0 }, { "distDrive", 0.2f }, { "distMix", 0.3f } }), mFm),
        P ("Guzheng", "World", with (string, { { "res", 0.988f }, { "noiseTone", 1.0f }, { "ampD", 2.4f }, { "mode", 2 }, { "glide", 0.08f }, { "verbMix", 0.3f } }), mString),
        P ("Oud", "World", with (string, { { "res", 0.975f }, { "noiseTone", 0.7f }, { "ampD", 1.2f }, { "fltEnv", 0.1f } }), mString),
        P ("Didgeridoo", "World", with (poly, { { "mode", 1 }, { "aTable", 0 }, { "aPos", 0.66f }, { "aOct", -1 }, { "ampS", 1.0f },
            { "fltType", 7 }, { "cutoff", 150 }, { "res", 0.6f }, { "fltEnv", 0 }, { "lfo1Rate", 1.5f }, { "lfo1Retrig", 0 },
            { "mod5Src", 1 }, { "mod5Dest", 8 }, { "mod5Amt", 0.35f } }), mPoly),
        P ("Hang Drum", "World", with (fm, { { "aPos", 0.1f }, { "bOct", 1 }, { "bSemi", 7 }, { "mod1Amt", 0.15f }, { "modD", 0.1f }, { "ampD", 2.0f },
            { "verbMix", 0.3f } }), mFm),
    };
}

} // namespace ab
