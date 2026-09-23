#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PresetTrims.h"

using namespace ab;

namespace
{
    using Float = juce::AudioParameterFloat;
    using Choice = juce::AudioParameterChoice;
    using Bool = juce::AudioParameterBool;
    using Int = juce::AudioParameterInt;

    juce::String pctText (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
    juce::String bipolarPct (float v, int) { const int p = juce::roundToInt (v * 100.0f); return (p > 0 ? "+" : "") + juce::String (p) + "%"; }
    juce::String timeText (float v, int)
    {
        if (v < 1.0f) return juce::String (juce::roundToInt (v * 1000.0f)) + " ms";
        return juce::String (v, 2) + " s";
    }
    juce::String hzText (float v, int)
    {
        if (v >= 1000.0f) return juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz";
        return juce::String (juce::roundToInt (v)) + " Hz";
    }
    juce::String lfoHzText (float v, int) { return juce::String (v, v < 1.0f ? 2 : 1) + " Hz"; }
    juce::String semiText (float v, int) { const int s = juce::roundToInt (v); return (s > 0 ? "+" : "") + juce::String (s) + " st"; }
    juce::String dbText (float v, int) { return juce::String (v, 1) + " dB"; }

    juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    template <typename P, typename... Args>
    void add (juce::AudioProcessorValueTreeState::ParameterLayout& layout, Args&&... args)
    {
        layout.add (std::make_unique<P> (std::forward<Args> (args)...));
    }

    void addFloat (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const juce::String& id, const juce::String& name,
                   juce::NormalisableRange<float> range, float def, juce::String (*text) (float, int))
    {
        layout.add (std::make_unique<Float> (juce::ParameterID { id, 1 }, name, range, def,
                                             juce::AudioParameterFloatAttributes().withStringFromValueFunction (text)));
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout HypernovaAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto pid = [] (const juce::String& id) { return juce::ParameterID { id, 1 }; };

    for (int o = 0; o < 2; ++o)
    {
        const juce::String p = o == 0 ? "a" : "b";
        const juce::String n = o == 0 ? "Osc A " : "Osc B ";
        add<Bool> (l, pid (p + "On"), n + "On", o == 0);
        add<Choice> (l, pid (p + "Table"), n + "Wavetable", WavetableBank::names(), 0);
        addFloat (l, p + "Pos", n + "Position", { 0.0f, 1.0f }, o == 0 ? 0.66f : 0.0f, pctText);
        add<Choice> (l, pid (p + "Warp"), n + "Warp Mode", warpNames(), 0);
        addFloat (l, p + "WarpAmt", n + "Warp", { 0.0f, 1.0f }, 0.0f, pctText);
        add<Int> (l, pid (p + "Uni"), n + "Unison", 1, MaxUnison, 1);
        addFloat (l, p + "Detune", n + "Detune", { 0.0f, 1.0f }, 0.2f, pctText);
        addFloat (l, p + "Blend", n + "Blend", { 0.0f, 1.0f }, 0.5f, pctText);
        addFloat (l, p + "Level", n + "Level", { 0.0f, 1.0f }, 0.75f, pctText);
        addFloat (l, p + "Pan", n + "Pan", { -1.0f, 1.0f }, 0.0f, bipolarPct);
        add<Int> (l, pid (p + "Oct"), n + "Octave", -3, 3, 0);
        add<Int> (l, pid (p + "Semi"), n + "Semitone", -12, 12, 0);
        addFloat (l, p + "Fine", n + "Fine", { -100.0f, 100.0f }, 0.0f, [] (float v, int) { return juce::String (juce::roundToInt (v)) + " ct"; });
        addFloat (l, p + "Width", n + "Unison Width", { 0.0f, 1.0f }, 1.0f, pctText);
        add<Bool> (l, pid (p + "Filter"), n + "To Filter", true);
    }

    add<Bool> (l, pid ("subOn"), "Sub On", false);
    add<Choice> (l, pid ("subShape"), "Sub Shape", subShapeNames(), 0);
    add<Int> (l, pid ("subOct"), "Sub Octave", -2, 0, -1);
    addFloat (l, "subLevel", "Sub Level", { 0.0f, 1.0f }, 0.6f, pctText);
    add<Bool> (l, pid ("subFilter"), "Sub To Filter", false);
    addFloat (l, "noiseLevel", "Noise Level", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "noiseTone", "Noise Tone", { 0.0f, 1.0f }, 0.7f, pctText);
    add<Choice> (l, pid ("noiseType"), "Noise Type", ab::dsp::noiseNames(), 0);
    add<Bool> (l, pid ("noiseFilter"), "Noise To Filter", true);

    addFloat (l, "xFmAB", "Cross FM A to B", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "xFmBA", "Cross FM B to A", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "xRing", "Ring Mod", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "xAm", "Amplitude Mod", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "xFltFm", "Filter FM", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "dropAmt", "Pitch Drop", { -48.0f, 48.0f, 1.0f }, 0.0f, semiText);
    addFloat (l, "dropTime", "Pitch Drop Time", skewed (0.002f, 1.0f, 0.06f), 0.05f, timeText);
    addFloat (l, "glide", "Glide", skewed (0.0f, 2.0f, 0.15f), 0.0f, timeText);
    add<Choice> (l, pid ("mode"), "Voice Mode", voiceModeNames(), 0);
    add<Int> (l, pid ("bendRange"), "Bend Range", 0, 24, 2);
    add<Bool> (l, pid ("retrig"), "Phase Retrigger", true);
    addFloat (l, "velSens", "Velocity Sensitivity", { 0.0f, 1.0f }, 0.5f, pctText);

    add<Bool> (l, pid ("fltOn"), "Filter On", true);
    add<Choice> (l, pid ("fltType"), "Filter Type", filterNames(), 1);
    addFloat (l, "cutoff", "Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 16000.0f, hzText);
    addFloat (l, "res", "Resonance", { 0.0f, 1.0f }, 0.1f, pctText);
    addFloat (l, "fltDrive", "Filter Drive", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "fltEnv", "Filter Env Amount", { -1.0f, 1.0f }, 0.0f, bipolarPct);
    addFloat (l, "fltKey", "Filter Key Track", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "fltMix", "Filter Mix", { 0.0f, 1.0f }, 1.0f, pctText);

    const auto att = skewed (0.001f, 5.0f, 0.3f), dec = skewed (0.005f, 10.0f, 0.8f);
    addFloat (l, "ampA", "Amp Attack", att, 0.001f, timeText);
    addFloat (l, "ampD", "Amp Decay", dec, 0.6f, timeText);
    addFloat (l, "ampS", "Amp Sustain", { 0.0f, 1.0f }, 1.0f, pctText);
    addFloat (l, "ampR", "Amp Release", dec, 0.15f, timeText);
    addFloat (l, "modA", "Mod Env Attack", att, 0.001f, timeText);
    addFloat (l, "modD", "Mod Env Decay", dec, 0.4f, timeText);
    addFloat (l, "modS", "Mod Env Sustain", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "modR", "Mod Env Release", dec, 0.3f, timeText);

    for (int i = 1; i <= 2; ++i)
    {
        const juce::String p = "lfo" + juce::String (i);
        const juce::String n = "LFO " + juce::String (i) + " ";
        add<Choice> (l, pid (p + "Shape"), n + "Shape", lfoShapeNames(), 0);
        addFloat (l, p + "Rate", n + "Rate", skewed (0.02f, 30.0f, 2.0f), 1.0f, lfoHzText);
        add<Choice> (l, pid (p + "Sync"), n + "Sync", lfoSyncNames(), 0);
        add<Bool> (l, pid (p + "Retrig"), n + "Retrigger", true);
        addFloat (l, p + "Fade", n + "Fade In", skewed (0.0f, 8.0f, 1.0f), 0.0f, timeText);
    }

    for (int i = 1; i <= NumModSlots; ++i)
    {
        const juce::String p = "mod" + juce::String (i);
        const juce::String n = "Mod " + juce::String (i) + " ";
        add<Choice> (l, pid (p + "Src"), n + "Source", modSrcNames(), 0);
        add<Choice> (l, pid (p + "Dest"), n + "Destination", modDestNames(), 0);
        addFloat (l, p + "Amt", n + "Amount", { -1.0f, 1.0f }, 0.0f, bipolarPct);
    }

    for (int i = 1; i <= 4; ++i)
        addFloat (l, "macro" + juce::String (i), "Macro " + juce::String (i), { 0.0f, 1.0f }, 0.0f, pctText);

    // Second effects page.
    add<Choice> (l, pid ("chorusMode"), "Chorus Mode", juce::StringArray { "Classic", "Ensemble", "Dimension" }, 0);
    add<Choice> (l, pid ("dlyStyle"), "Delay Style", juce::StringArray { "Digital", "Reverse", "Granular" }, 0);
    add<Choice> (l, pid ("verbMode"), "Reverb Mode", juce::StringArray { "Space", "Plate", "Spring", "Room" }, 0);
    addFloat (l, "flangMix", "Flanger Mix", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "flangRate", "Flanger Rate", skewed (0.02f, 8.0f, 0.6f), 0.3f, lfoHzText);
    addFloat (l, "flangDepth", "Flanger Depth", { 0.0f, 1.0f }, 0.5f, pctText);
    addFloat (l, "flangFb", "Flanger Feedback", { -1.0f, 1.0f }, 0.4f, bipolarPct);
    addFloat (l, "tapeWow", "Tape Wobble", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "tapeNoise", "Tape Noise", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "tapeSat", "Tape Saturation", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "gateDepth", "Gate Depth", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "gateShape", "Gate Shape", { 0.0f, 1.0f }, 0.3f, pctText);
    add<Choice> (l, pid ("gateRate"), "Gate Rate", ab::dsp::syncRateNames(), 6);
    add<Choice> (l, pid ("gatePattern"), "Gate Pattern", ab::dsp::GateAndPan::patternNames(), 0);
    addFloat (l, "panDepth", "Auto Pan Depth", { 0.0f, 1.0f }, 0.0f, pctText);
    add<Choice> (l, pid ("panRate"), "Auto Pan Rate", ab::dsp::syncRateNames(), 4);
    add<Choice> (l, pid ("fxFltType"), "FX Filter Type", ab::dsp::FxFilter::typeNames(), 0);
    addFloat (l, "fxFltFreq", "FX Filter Freq", skewed (20.0f, 20000.0f, 1000.0f), 20000.0f, hzText);
    addFloat (l, "fxFltRes", "FX Filter Resonance", { 0.0f, 1.0f }, 0.2f, pctText);
    addFloat (l, "fxFltDepth", "FX Filter Sweep", { 0.0f, 1.0f }, 0.0f, pctText);
    add<Choice> (l, pid ("fxFltRate"), "FX Filter Rate", ab::dsp::syncRateNames(), 4);
    addFloat (l, "shiftSemis", "Pitch Shift", { -12.0f, 12.0f, 1.0f }, 0.0f, semiText);
    addFloat (l, "shiftMix", "Pitch Shift Mix", { 0.0f, 1.0f }, 0.0f, pctText);
    for (auto* fx : { "flang", "tape", "gate", "fxFlt", "shift" })
        add<Bool> (l, pid (juce::String (fx) + "On"), juce::String (fx) + " On", true);
    for (auto* fx : { "dist", "ott", "chorus", "dly", "verb", "eq" })
        add<Bool> (l, pid (juce::String (fx) + "On"), juce::String (fx).substring (0, 1).toUpperCase() + juce::String (fx).substring (1) + " On", true);
    add<Choice> (l, pid ("distType"), "Distortion Type", distNames(), 0);
    addFloat (l, "distDrive", "Distortion Drive", { 0.0f, 1.0f }, 0.3f, pctText);
    addFloat (l, "distMix", "Distortion Mix", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "ott", "Multiband Squash", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "chorusMix", "Chorus Mix", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "chorusRate", "Chorus Rate", skewed (0.05f, 5.0f, 0.8f), 0.6f, lfoHzText);
    add<Choice> (l, pid ("dlyTime"), "Delay Time", delayTimeNames(), 5);
    addFloat (l, "dlyFb", "Delay Feedback", { 0.0f, 0.95f }, 0.35f, pctText);
    addFloat (l, "dlyMix", "Delay Mix", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "verbSize", "Reverb Size", { 0.0f, 1.0f }, 0.6f, pctText);
    addFloat (l, "verbMix", "Reverb Mix", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "verbShimmer", "Reverb Shimmer", { 0.0f, 1.0f }, 0.0f, pctText);
    add<Bool> (l, pid ("monoBass"), "Mono Bass", true);
    add<Bool> (l, pid ("dlyPing"), "Delay Ping-Pong", true);
    addFloat (l, "dlyTone", "Delay Tone", { 0.0f, 1.0f }, 0.6f, pctText);
    addFloat (l, "width", "Stereo Width", { 0.0f, 2.0f }, 1.0f, pctText);
    addFloat (l, "eqLow", "EQ Low", { -12.0f, 12.0f, 0.1f }, 0.0f, dbText);
    addFloat (l, "eqHigh", "EQ High", { -12.0f, 12.0f, 0.1f }, 0.0f, dbText);

    add<Int> (l, pid ("transpose"), "Transpose", -24, 24, 0);
    addFloat (l, "tune", "Fine Tune", { -100.0f, 100.0f }, 0.0f, [] (float v, int) { return juce::String (juce::roundToInt (v)) + " ct"; });
    addFloat (l, "drift", "Analog Drift", { 0.0f, 1.0f }, 0.0f, pctText);
    add<Choice> (l, pid ("chord"), "Chord", chordNames(), 0);
    addFloat (l, "strum", "Chord Strum", skewed (0.0f, 0.12f, 0.03f), 0.0f, timeText);
    add<Choice> (l, pid ("quality"), "Quality", juce::StringArray { "Eco", "High", "Ultra" }, 1);
    add<Bool> (l, pid ("arpOn"), "Arp On", false);
    add<Choice> (l, pid ("arpMode"), "Arp Mode", arpModeNames(), 0);
    add<Choice> (l, pid ("arpRate"), "Arp Rate", arpRateNames(), 3);
    add<Int> (l, pid ("arpOct"), "Arp Octaves", 1, 4, 1);
    addFloat (l, "arpGate", "Arp Gate", { 0.05f, 1.0f }, 0.6f, pctText);
    addFloat (l, "volume", "Master Volume", { -36.0f, 12.0f, 0.1f }, -6.0f, dbText);

    // Sampler (appended: existing IDs and their order stay put).
    {
        juce::NormalisableRange<float> sAtt (0.001f, 5.0f), sDec (0.005f, 10.0f);
        sAtt.setSkewForCentre (0.15f);
        sDec.setSkewForCentre (0.8f);
        add<Bool> (l, pid ("smpOn"), "Sampler On", false);
        addFloat (l, "smpLevel", "Sampler Level", { 0.0f, 1.0f }, 0.8f, pctText);
        addFloat (l, "smpPan", "Sampler Pan", { -1.0f, 1.0f }, 0.0f, bipolarPct);
        l.add (std::make_unique<Int> (pid ("smpRoot"), "Sampler Root Note", 0, 127, 60,
                                      juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                                      { return juce::MidiMessage::getMidiNoteName (v, true, true, 3); })));
        add<Bool> (l, pid ("smpTrack"), "Sampler Key Tracking", true);
        addFloat (l, "smpSemi", "Sampler Semitone", { -24.0f, 24.0f, 1.0f }, 0.0f, semiText);
        addFloat (l, "smpFine", "Sampler Fine", { -100.0f, 100.0f }, 0.0f, [] (float v, int) { return juce::String (juce::roundToInt (v)) + " ct"; });
        addFloat (l, "smpStart", "Sampler Start", { 0.0f, 1.0f }, 0.0f, pctText);
        addFloat (l, "smpEnd", "Sampler End", { 0.0f, 1.0f }, 1.0f, pctText);
        add<Choice> (l, pid ("smpLoop"), "Sampler Loop", ab::sampleLoopNames(), 0);
        addFloat (l, "smpLoopStart", "Sampler Loop Start", { 0.0f, 1.0f }, 0.25f, pctText);
        addFloat (l, "smpLoopEnd", "Sampler Loop End", { 0.0f, 1.0f }, 1.0f, pctText);
        add<Bool> (l, pid ("smpReverse"), "Sampler Reverse", false);
        add<Bool> (l, pid ("smpFilter"), "Sampler To Filter", true);
        addFloat (l, "smpA", "Sampler Attack", sAtt, 0.001f, timeText);
        addFloat (l, "smpD", "Sampler Decay", sDec, 0.6f, timeText);
        addFloat (l, "smpS", "Sampler Sustain", { 0.0f, 1.0f }, 1.0f, pctText);
        addFloat (l, "smpR", "Sampler Release", sDec, 0.25f, timeText);
    }

    // Oscillators C to H (appended, off by default): the same controls as A and B.
    for (int o = 2; o < NumOsc; ++o)
    {
        const juce::String p = oscPrefix (o);
        const juce::String n = "Osc " + p.toUpperCase() + " ";
        add<Bool> (l, pid (p + "On"), n + "On", false);
        add<Choice> (l, pid (p + "Table"), n + "Wavetable", WavetableBank::names(), 0);
        addFloat (l, p + "Pos", n + "Position", { 0.0f, 1.0f }, 0.66f, pctText);
        add<Choice> (l, pid (p + "Warp"), n + "Warp Mode", warpNames(), 0);
        addFloat (l, p + "WarpAmt", n + "Warp", { 0.0f, 1.0f }, 0.0f, pctText);
        add<Int> (l, pid (p + "Uni"), n + "Unison", 1, MaxUnison, 1);
        addFloat (l, p + "Detune", n + "Detune", { 0.0f, 1.0f }, 0.2f, pctText);
        addFloat (l, p + "Blend", n + "Blend", { 0.0f, 1.0f }, 0.5f, pctText);
        addFloat (l, p + "Level", n + "Level", { 0.0f, 1.0f }, 0.6f, pctText);
        addFloat (l, p + "Pan", n + "Pan", { -1.0f, 1.0f }, 0.0f, bipolarPct);
        add<Int> (l, pid (p + "Oct"), n + "Octave", -3, 3, 0);
        add<Int> (l, pid (p + "Semi"), n + "Semitone", -12, 12, 0);
        addFloat (l, p + "Fine", n + "Fine", { -100.0f, 100.0f }, 0.0f, [] (float v, int) { return juce::String (juce::roundToInt (v)) + " ct"; });
        addFloat (l, p + "Width", n + "Unison Width", { 0.0f, 1.0f }, 1.0f, pctText);
        add<Bool> (l, pid (p + "Filter"), n + "To Filter", true);
    }

    // EQ bands (appended; the defaults are the old fixed shelves, so older sounds don't change).
    addFloat (l, "eqLowFreq", "EQ Low Freq", skewed (30.0f, 600.0f, 140.0f), 120.0f, hzText);
    addFloat (l, "eqHighFreq", "EQ High Freq", skewed (1000.0f, 16000.0f, 5000.0f), 5000.0f, hzText);
    addFloat (l, "eqMidFreq", "EQ Mid Freq", skewed (60.0f, 12000.0f, 1000.0f), 1000.0f, hzText);
    addFloat (l, "eqMidGain", "EQ Mid", { -18.0f, 18.0f, 0.1f }, 0.0f, dbText);
    addFloat (l, "eqMidQ", "EQ Mid Q", skewed (0.3f, 8.0f, 1.2f), 1.0f, [] (float v, int) { return juce::String (v, 2); });
    addFloat (l, "eqLowCut", "EQ Low Cut", skewed (20.0f, 1000.0f, 120.0f), 20.0f, [] (float v, int) { return v <= 20.5f ? juce::String ("Off") : hzText (v, 0); });
    addFloat (l, "eqHighCut", "EQ High Cut", skewed (1000.0f, 20000.0f, 8000.0f), 20000.0f, [] (float v, int) { return v >= 19900.0f ? juce::String ("Off") : hzText (v, 0); });

    // Low End (appended).
    add<Bool> (l, pid ("lowOn"), "Low End On", false);
    addFloat (l, "lowXover", "Low End Crossover", skewed (40.0f, 300.0f, 110.0f), 120.0f, hzText);
    addFloat (l, "lowLevel", "Low End Level", { -12.0f, 6.0f, 0.1f }, 0.0f, dbText);
    addFloat (l, "lowDrive", "Low End Warmth", { 0.0f, 1.0f }, 0.0f, pctText);
    add<Bool> (l, pid ("lowMono"), "Low End Mono", true);
    addFloat (l, "lowDuck", "Low End Duck", { 0.0f, 1.0f }, 0.0f, pctText);
    add<Choice> (l, pid ("lowDuckRate"), "Low End Duck Rate", juce::StringArray { "1/4", "1/8", "1/2", "1 bar", "1/16" }, 0);
    addFloat (l, "lowDuckRelease", "Low End Duck Release", skewed (0.02f, 0.6f, 0.15f), 0.15f, timeText);

    // How each modulation slot is shaped and how fast it's allowed to move (appended).
    for (int i = 1; i <= NumModSlots; ++i)
    {
        const juce::String p = "mod" + juce::String (i);
        add<Choice> (l, pid (p + "Shape"), "Mod " + juce::String (i) + " Shape", modShapeNames(), 0);
        addFloat (l, p + "Smooth", "Mod " + juce::String (i) + " Smoothing", { 0.0f, 1.0f }, 0.0f, pctText);
    }

    // Crush (appended): bit and sample-rate reduction as a rack effect.
    add<Bool> (l, pid ("crushOn"), "Crush On", true);
    addFloat (l, "crushBits", "Crush Bits", { 1.0f, 16.0f, 0.1f }, 16.0f, [] (float v, int) { return juce::String (v, 1) + " bit"; });
    addFloat (l, "crushRate", "Crush Rate", skewed (200.0f, 24000.0f, 6000.0f), 24000.0f, hzText);
    addFloat (l, "crushMix", "Crush Mix", { 0.0f, 1.0f }, 0.0f, pctText);

    // Speaker (appended): the box the sound comes out of.
    add<Bool> (l, pid ("spkOn"), "Speaker On", true);
    add<Choice> (l, pid ("spkType"), "Speaker", dsp::Speaker::typeNames(), 0);
    addFloat (l, "spkDrive", "Speaker Grit", { 0.0f, 1.0f }, 0.3f, pctText);
    addFloat (l, "spkMix", "Speaker Mix", { 0.0f, 1.0f }, 0.0f, pctText);

    // MPE (appended): each note on its own channel, with its own bend, pressure and slide.
    add<Bool> (l, pid ("mpeOn"), "MPE", false);
    addFloat (l, "mpeBend", "MPE Bend Range", { 1.0f, 96.0f, 1.0f }, 48.0f, [] (float v, int) { return juce::String (juce::roundToInt (v)) + " st"; });

    // Envelope follower (appended): how loud the synth itself is, as a modulation source.
    addFloat (l, "folAtt", "Follower Attack", skewed (0.5f, 200.0f, 20.0f), 10.0f, [] (float v, int) { return juce::String (v, 1) + " ms"; });
    addFloat (l, "folRel", "Follower Release", skewed (10.0f, 2000.0f, 250.0f), 200.0f, [] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; });
    addFloat (l, "folGain", "Follower Sensitivity", { 0.25f, 4.0f }, 1.0f, [] (float v, int) { return juce::String (v, 2) + "x"; });

    // One cycle per note for each LFO (appended): a drawn shape then works as an envelope.
    for (int i = 1; i <= NumLfo; ++i)
        add<Bool> (l, pid ("lfo" + juce::String (i) + "Once"), "LFO " + juce::String (i) + " Once", false);

    // The rack's own dry/wet (appended): 100% is the whole rack, lower blends the sound going in back over it.
    addFloat (l, "fxMix", "Rack Mix", { 0.0f, 1.0f }, 1.0f, pctText);

    // LFO 3 and 4 (appended): the same controls as LFO 1 and 2. They run whether or not their widget is on screen.
    for (int i = 3; i <= NumLfo; ++i)
    {
        const juce::String p = "lfo" + juce::String (i);
        const juce::String n = "LFO " + juce::String (i) + " ";
        add<Choice> (l, pid (p + "Shape"), n + "Shape", lfoShapeNames(), 0);
        addFloat (l, p + "Rate", n + "Rate", skewed (0.02f, 30.0f, 2.0f), 1.0f, lfoHzText);
        add<Choice> (l, pid (p + "Sync"), n + "Sync", lfoSyncNames(), 0);
        add<Bool> (l, pid (p + "Retrig"), n + "Retrigger", true);
        addFloat (l, p + "Fade", n + "Fade In", skewed (0.0f, 8.0f, 1.0f), 0.0f, timeText);
    }

    // Chop Lab (appended): the sample cut into slices, one per key.
    add<Bool> (l, pid ("chopOn"), "Chop", false);
    add<Int> (l, pid ("chopRoot"), "Chop Root Key", 0, 127, 60);
    add<Bool> (l, pid ("chopHold"), "Chop Hold", false);

    // Orbit (appended): four captured sounds at the corners of a square, and a point that morphs between them.
    add<Bool> (l, pid ("orbitOn"), "Orbit", false);
    addFloat (l, "orbitX", "Orbit X", { 0.0f, 1.0f }, 0.0f, pctText);
    addFloat (l, "orbitY", "Orbit Y", { 0.0f, 1.0f }, 0.0f, pctText);
    add<Choice> (l, pid ("orbitPath"), "Orbit Path", Orbit::pathNames(), 0);
    addFloat (l, "orbitRate", "Orbit Rate", skewed (0.01f, 8.0f, 0.3f), 0.2f, lfoHzText);
    add<Choice> (l, pid ("orbitSync"), "Orbit Sync", lfoSyncNames(), 0);
    addFloat (l, "orbitDepth", "Orbit Travel", { 0.0f, 1.0f }, 0.3f, pctText);

    // Routing (appended): every source plays into a bus and every effect sits on one, so two chains can run
    // side by side and meet at the output. Everything defaults to the main bus, so older sounds are unchanged.
    {
        const juce::StringArray busNames { "Main", "Alt" };
        for (int o = 0; o < NumOsc; ++o)
            add<Choice> (l, pid (oscPrefix (o) + "Bus"), "Osc " + oscPrefix (o).toUpperCase() + " Bus", busNames, 0);
        add<Choice> (l, pid ("subBus"), "Sub Bus", busNames, 0);
        add<Choice> (l, pid ("noiseBus"), "Noise Bus", busNames, 0);
        add<Choice> (l, pid ("smpBus"), "Sampler Bus", busNames, 0);
        for (int fx = 0; fx < NumFx; ++fx)
            add<Choice> (l, pid (fxParamPrefix (fx) + "Bus"), fxRackNames()[fx] + " Bus", busNames, 0);
    }

    return l;
}

//==============================================================================
HypernovaAudioProcessor::HypernovaAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "ArrowBass", createLayout()) // state tag kept from the Arrow Bass days so old sessions load
{
    WavetableBank::get(); // build the tables up front, not on the audio thread
    changeCounter = std::make_unique<ChangeCounter> (parameterChanges);
    for (int d = FirstFxParamDest; d < NumDest; ++d) fxModParam[(size_t) d] = apvts.getParameter (modDestParam (d));
    orderListener = std::make_unique<OrderListener> (*this);
    apvts.state.addListener (orderListener.get());
    syncFxOrder();
    syncLfoTables();
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.addParameterListener (rp->getParameterID(), changeCounter.get());
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto id = rp->getParameterID();
            raw[id.toStdString()] = (int) rawValue.size();
            rawValue.push_back (apvts.getRawParameterValue (id));
            rawParam.push_back (rp);
            // A choice, a switch or a whole number can only be one thing or another, so Orbit takes the
            // nearest corner's value for it instead of landing somewhere in between.
            rawDiscrete.push_back ((juce::uint8) (dynamic_cast<juce::AudioParameterChoice*> (rp) != nullptr
                                                  || dynamic_cast<juce::AudioParameterBool*> (rp) != nullptr
                                                  || dynamic_cast<juce::AudioParameterInt*> (rp) != nullptr));
            // What Orbit leaves alone: its own controls, the macros and the mod wheel (how you play the sound,
            // not part of it), and the machine settings. Everything else is the sound and gets blended.
            rawMorphs.push_back ((juce::uint8) ! (id.startsWith ("orbit") || id.startsWith ("macro")
                                                  || id == "quality" || id.startsWith ("mpe")));
        }
    morphed = std::vector<std::atomic<float>> (rawValue.size());
    for (size_t i = 0; i < rawValue.size(); ++i) morphed[i].store (rawValue[i]->load(), std::memory_order_relaxed);
    // Each effect's bus is read every block, so its parameter is looked up once here instead.
    for (int fx = 0; fx < NumFx; ++fx)
        fxBusRaw[(size_t) fx] = apvts.getRawParameterValue (fxParamPrefix (fx) + "Bus");
}

// Is anything actually playing into the alt bus? Effects parked there do nothing until something is.
static bool altBusInUse (const ab::SynthSettings& s)
{
    for (int o = 0; o < ab::NumOsc; ++o)
        if (s.osc[(size_t) o].on && s.osc[(size_t) o].bus != 0) return true;
    if (s.subOn && s.subBus != 0) return true;
    if (s.noiseLevel > 0.0001f && s.noiseBus != 0) return true;
    if (s.smp.on && s.smp.bus != 0) return true;
    return false;
}

float HypernovaAudioProcessor::param (const char* id) const
{
    auto it = raw.find (id);
    jassert (it != raw.end());
    if (it == raw.end()) return 0.0f;
    const size_t i = (size_t) it->second;
    // With Orbit live the engine hears the blend of the captured sounds, not the knobs as they sit.
    if (morphActive.load (std::memory_order_relaxed) && rawMorphs[i] != 0) return morphed[i].load (std::memory_order_relaxed);
    return rawValue[i]->load (std::memory_order_relaxed);
}

void HypernovaAudioProcessor::setParam (const juce::String& id, float realValue)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
    else
        jassertfalse;
}

bool HypernovaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

HypernovaAudioProcessor::~HypernovaAudioProcessor()
{
    apvts.state.removeListener (orderListener.get());
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.removeParameterListener (rp->getParameterID(), changeCounter.get());
}

void HypernovaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateNow = sampleRate;
    maxBlock = juce::jmax (32, samplesPerBlock);
    // A small phone speaker: nothing much under ~300 Hz, a honky bump around 2.5 kHz, rolled off up top.
    for (int c = 0; c < 2; ++c)
    {
        speakerHp[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 320.0, 0.8);
        speakerHp2[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 320.0, 0.6);
        speakerBump[c].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 2500.0, 1.2, 1.8f);
        speakerLp[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, juce::jmin (9000.0, sampleRate * 0.45), 0.7);
        for (auto* f : { &speakerHp[c], &speakerHp2[c], &speakerBump[c], &speakerLp[c] }) f->reset();
    }
    for (int i = 0; i < 2; ++i)
        for (auto* set : { &voiceOversampler, &altOversampler })
        {
            (*set)[(size_t) i] = std::make_unique<juce::dsp::Oversampling<float>> (2, (size_t) (i + 1),
                                     juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
            (*set)[(size_t) i]->initProcessing ((size_t) maxBlock);
        }
    altBuf.setSize (2, maxBlock);
    altBuf.clear();
    limiterLen = juce::jlimit (1, 512, juce::roundToInt (0.0015 * sampleRate));
    limTarget.assign ((size_t) limiterLen, 1.0f);
    limHeld.assign ((size_t) limiterLen, 1.0f);
    limDelayL.assign ((size_t) limiterLen, 0.0f);
    limDelayR.assign ((size_t) limiterLen, 0.0f);
    limiterPos = limiterLoud = 0;
    limiterSum = limiterLen;
    limiterOutGain = 1.0f;
    currentQuality = -1;
    applyQuality ((int) param ("quality"));
    smoothReady = false;
    limiterGain = 1.0f;
    effects.prepare (sampleRate, maxBlock);
    masterGain.reset (sampleRate, 0.05);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (param ("volume")));
    heldNotes.clear();
    heldNotes.reserve (128);
    midiOrder.reserve (2048);
    keyboardState.reset();
}

//==============================================================================
SynthSettings HypernovaAudioProcessor::readSynthSettings()
{
    SynthSettings s;
    for (int o = 0; o < NumOsc; ++o)
    {
        const std::string p = oscPrefix (o).toStdString();
        auto get = [&] (const char* suffix) { return param ((p + suffix).c_str()); };
        auto& os = s.osc[(size_t) o];
        os.on = get ("On") > 0.5f;
        os.table = (int) get ("Table");
        os.custom = userTable[(size_t) o].load();
        os.pos = get ("Pos");
        os.warp = (int) get ("Warp");
        os.warpAmt = get ("WarpAmt");
        os.unison = (int) get ("Uni");
        os.detune = get ("Detune");
        os.blend = get ("Blend");
        os.level = get ("Level");
        os.pan = get ("Pan");
        os.pitch = get ("Oct") * 12.0f + get ("Semi") + get ("Fine") * 0.01f;
        os.toFilter = get ("Filter") > 0.5f;
        os.bus = (int) get ("Bus");
    }
    s.subOn = param ("subOn") > 0.5f;
    s.subShape = (int) param ("subShape");
    s.subOct = (int) param ("subOct");
    s.subLevel = param ("subLevel");
    s.subToFilter = param ("subFilter") > 0.5f;
    s.subBus = (int) param ("subBus");
    s.noiseLevel = param ("noiseLevel");
    s.noiseTone = param ("noiseTone");
    s.noiseType = (int) param ("noiseType");
    s.noiseToFilter = param ("noiseFilter") > 0.5f;
    s.noiseBus = (int) param ("noiseBus");
    s.xFmAB = param ("xFmAB");
    s.xFmBA = param ("xFmBA");
    s.xRing = param ("xRing");
    s.xAm = param ("xAm");
    s.xFltFm = param ("xFltFm");
    s.pitchEnvAmt = param ("dropAmt");
    s.pitchEnvDecay = param ("dropTime");
    s.glide = param ("glide");
    s.mode = (int) param ("mode");
    s.phaseRetrig = param ("retrig") > 0.5f;
    s.velSens = param ("velSens");
    s.transpose = param ("transpose") + param ("tune") * 0.01f;
    s.drift = param ("drift");
    s.osc[0].width = param ("aWidth");
    s.osc[1].width = param ("bWidth");
    s.filterOn = param ("fltOn") > 0.5f;
    s.filterType = (int) param ("fltType");
    s.cutoff = param ("cutoff");
    s.res = param ("res");
    s.filterDrive = param ("fltDrive");
    s.filterEnv = param ("fltEnv");
    s.keytrack = param ("fltKey");
    s.filterMix = param ("fltMix");
    s.env[0] = { param ("ampA"), param ("ampD"), param ("ampS"), param ("ampR") };
    s.env[1] = { param ("modA"), param ("modD"), param ("modS"), param ("modR") };
    for (int i = 0; i < NumLfo; ++i)
    {
        const std::string p = "lfo" + std::to_string (i + 1);
        auto& ls = s.lfo[(size_t) i];
        ls.table = lfoTables[(size_t) lfoTableSide[(size_t) i].load (std::memory_order_acquire)][(size_t) i].data();
        ls.shape = (int) param ((p + "Shape").c_str());
        const int sync = (int) param ((p + "Sync").c_str());
        ls.rateHz = sync == 0 ? param ((p + "Rate").c_str()) : (float) (bpm / 60.0 / lfoSyncBeats (sync));
        ls.retrig = param ((p + "Retrig").c_str()) > 0.5f;
        ls.once = param ((p + "Once").c_str()) > 0.5f;
        ls.drawnEnd = ls.shape == LDrawn ? lfoDrawnEnd[(size_t) i].load() : 1.0f;
        ls.fade = param ((p + "Fade").c_str());
    }
    for (int i = 0; i < NumModSlots; ++i)
    {
        const std::string p = "mod" + std::to_string (i + 1);
        s.mod[(size_t) i] = { (int) param ((p + "Src").c_str()), (int) param ((p + "Dest").c_str()), param ((p + "Amt").c_str()),
                              (int) param ((p + "Shape").c_str()), param ((p + "Smooth").c_str()) };
    }
    auto& sm = s.smp;
    sm.data = currentSample.load (std::memory_order_acquire);
    sm.on = param ("smpOn") > 0.5f && sm.data != nullptr;
    sm.level = param ("smpLevel");
    sm.pan = param ("smpPan");
    sm.root = (int) param ("smpRoot");
    sm.track = param ("smpTrack") > 0.5f;
    sm.semi = param ("smpSemi");
    sm.fine = param ("smpFine");
    sm.start = param ("smpStart");
    sm.end = param ("smpEnd");
    sm.loop = (int) param ("smpLoop");
    sm.loopStart = param ("smpLoopStart");
    sm.loopEnd = param ("smpLoopEnd");
    sm.reverse = param ("smpReverse") > 0.5f;
    sm.toFilter = param ("smpFilter") > 0.5f;
    sm.bus = (int) param ("smpBus");
    sm.chop = param ("chopOn") > 0.5f;
    sm.chopRoot = (int) param ("chopRoot");
    sm.chopHold = param ("chopHold") > 0.5f;
    sm.slices = sliceTable[(size_t) sliceSide.load (std::memory_order_acquire)];
    sm.a = param ("smpA");
    sm.d = param ("smpD");
    sm.s = param ("smpS");
    sm.r = param ("smpR");
    return s;
}

// Knob moves and automation glide (~15 ms) instead of stepping once per block. Preset loads jump straight there.
void HypernovaAudioProcessor::smoothSettings (SynthSettings& s, int numSamples)
{
    const int version = presetVersion.load();
    if (! smoothReady || version != smoothVersion)
    {
        smoothed = s;
        smoothReady = true;
        smoothVersion = version;
        return;
    }
    const float a = 1.0f - std::exp (-(float) numSamples / (0.015f * (float) sampleRateNow));
    auto glide = [a] (float& cur, float target) { cur += (target - cur) * a; return cur; };
    for (int o = 0; o < NumOsc; ++o)
    {
        auto& d = s.osc[(size_t) o];
        auto& m = smoothed.osc[(size_t) o];
        d.pos = glide (m.pos, d.pos);
        d.warpAmt = glide (m.warpAmt, d.warpAmt);
        d.level = glide (m.level, d.level);
        d.pan = glide (m.pan, d.pan);
        d.detune = glide (m.detune, d.detune);
        d.blend = glide (m.blend, d.blend);
        d.width = glide (m.width, d.width);
    }
    s.xFmAB = glide (smoothed.xFmAB, s.xFmAB);
    s.xFmBA = glide (smoothed.xFmBA, s.xFmBA);
    s.xRing = glide (smoothed.xRing, s.xRing);
    s.xAm = glide (smoothed.xAm, s.xAm);
    s.xFltFm = glide (smoothed.xFltFm, s.xFltFm);
    s.subLevel = glide (smoothed.subLevel, s.subLevel);
    s.noiseLevel = glide (smoothed.noiseLevel, s.noiseLevel);
    s.noiseTone = glide (smoothed.noiseTone, s.noiseTone);
    s.smp.level = glide (smoothed.smp.level, s.smp.level);
    s.smp.pan = glide (smoothed.smp.pan, s.smp.pan);
    {
        // cutoff glides in octaves so sweeps sound even
        float lc = std::log2 (smoothed.cutoff);
        smoothed.cutoff = std::exp2 (glide (lc, std::log2 (juce::jmax (20.0f, s.cutoff))));
        s.cutoff = smoothed.cutoff;
    }
    s.res = glide (smoothed.res, s.res);
    s.filterDrive = glide (smoothed.filterDrive, s.filterDrive);
    s.filterEnv = glide (smoothed.filterEnv, s.filterEnv);
    s.filterMix = glide (smoothed.filterMix, s.filterMix);
}

FxSettings HypernovaAudioProcessor::readFxSettings()
{
    FxSettings f;
    {
        const auto packed = fxOrderPacked.load (std::memory_order_relaxed);
        for (int i = 0; i < NumFx; ++i) f.order[(size_t) i] = (juce::uint8) ((packed >> (4 * i)) & 0xf);
    }
    f.distType = (int) param ("distType");
    f.distDrive = param ("distDrive");
    f.distMix = param ("distMix");
    f.ott = param ("ott");
    f.chorusMix = param ("chorusMix");
    f.chorusRate = param ("chorusRate");
    f.delayTime = (int) param ("dlyTime");
    f.delayFeedback = param ("dlyFb");
    f.delayMix = param ("dlyMix");
    f.reverbSize = param ("verbSize");
    f.reverbMix = param ("verbMix");
    f.shimmer = param ("verbShimmer");
    f.monoBass = param ("monoBass") > 0.5f;
    f.delayPing = param ("dlyPing") > 0.5f;
    f.delayTone = param ("dlyTone");
    f.width = param ("width");
    f.rackMix = param ("fxMix");
    for (int fx = 0; fx < NumFx; ++fx)
        if (auto* bp = fxBusRaw[(size_t) fx])
            f.bus[(size_t) fx] = (juce::uint8) juce::jlimit (0, NumBus - 1, (int) bp->load (std::memory_order_relaxed));
    f.crushOn = param ("crushOn") > 0.5f;
    f.crushBits = param ("crushBits");
    f.crushRate = param ("crushRate");
    f.crushMix = param ("crushMix");
    f.speakerOn = param ("spkOn") > 0.5f;
    f.speakerType = (int) param ("spkType");
    f.speakerDrive = param ("spkDrive");
    f.speakerMix = param ("spkMix");
    f.distOn = param ("distOn") > 0.5f;
    f.ottOn = param ("ottOn") > 0.5f;
    f.chorusOn = param ("chorusOn") > 0.5f;
    f.delayOn = param ("dlyOn") > 0.5f;
    f.reverbOn = param ("verbOn") > 0.5f;
    f.eqOn = param ("eqOn") > 0.5f;
    f.chorusMode = (int) param ("chorusMode");
    f.delayStyle = (int) param ("dlyStyle");
    f.reverbMode = (int) param ("verbMode");
    f.flangerMix = param ("flangMix");
    f.flangerRate = param ("flangRate");
    f.flangerDepth = param ("flangDepth");
    f.flangerFeedback = param ("flangFb");
    f.tapeWobble = param ("tapeWow");
    f.tapeNoise = param ("tapeNoise");
    f.tapeSat = param ("tapeSat");
    f.gateDepth = param ("gateDepth");
    f.gateShape = param ("gateShape");
    f.gateRate = (int) param ("gateRate");
    f.gatePattern = (int) param ("gatePattern");
    f.panDepth = param ("panDepth");
    f.panRate = (int) param ("panRate");
    f.fxFilterType = (int) param ("fxFltType");
    f.fxFilterFreq = param ("fxFltFreq");
    f.fxFilterRes = param ("fxFltRes");
    f.fxFilterDepth = param ("fxFltDepth");
    f.fxFilterRate = (int) param ("fxFltRate");
    f.pitchSemis = param ("shiftSemis");
    f.pitchMix = param ("shiftMix");
    f.flangerOn = param ("flangOn") > 0.5f;
    f.tapeOn = param ("tapeOn") > 0.5f;
    f.gateOn = param ("gateOn") > 0.5f;
    f.fxFilterOn = param ("fxFltOn") > 0.5f;
    f.pitchOn = param ("shiftOn") > 0.5f;
    f.eqLowFreq = param ("eqLowFreq");
    f.eqHighFreq = param ("eqHighFreq");
    f.eqMidFreq = param ("eqMidFreq");
    f.eqMidGain = param ("eqMidGain");
    f.eqMidQ = param ("eqMidQ");
    f.eqLowCut = param ("eqLowCut");
    f.eqHighCut = param ("eqHighCut");
    f.lowOn = param ("lowOn") > 0.5f;
    f.lowXover = param ("lowXover");
    f.lowLevelDb = param ("lowLevel");
    f.lowDrive = param ("lowDrive");
    f.lowMono = param ("lowMono") > 0.5f;
    f.lowDuck = param ("lowDuck");
    f.lowDuckRate = (int) param ("lowDuckRate");
    f.lowDuckRelease = param ("lowDuckRelease");
    f.eqLow = param ("eqLow");
    f.eqHigh = param ("eqHigh");
    f.bpm = bpm;
    f.ppq = hostPpq;
    f.playing = hostPlaying;
    return f;
}

//==============================================================================
void HypernovaAudioProcessor::noteOn (int note, float velocity, int channel)
{
    const auto& s = blockSettings;
    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
    heldNotes.push_back (note);
    ++noteCounter;

    if (s.mode == ModePoly)
    {
        // Glide from the last note only while something is still sounding, so a fresh phrase starts on pitch.
        bool sounding = false;
        for (auto& v : voices) sounding = sounding || v.isActive();
        const bool glide = s.glide > 0.0005f && sounding && lastNote >= 0;
        const auto& chord = chordIntervals ((int) param ("chord"));
        const int strumSamples = (int) (param ("strum") * sampleRateNow);
        for (size_t k = 0; k < chord.size(); ++k)
        {
            const int n = note + chord[k];
            if (n > 127) break;
            auto& v = allocateVoice();
            sustained[(size_t) (&v - voices.data())] = false;
            v.start (n, velocity, glide ? (float) (lastNote + chord[k]) : -1.0f, true, s, globalMod, noteCounter, (int) k * strumSamples);
            v.trigger = note;
            v.channel = channel;
            const auto& e = channelExpression[(size_t) juce::jlimit (1, 16, channel)];
            v.noteBend = mpeOn() ? e.bend : 0.0f;
            v.pressure = e.pressure;
            v.slide = e.slide;
        }
    }
    else
    {
        auto& v = voices[(size_t) monoVoice];
        const bool overlapping = heldNotes.size() > 1 && v.isActive();
        // Legato slides without a new attack only while the note is still ringing. Once a plucky sound
        // (log drum, knock 808) has decayed, an overlapping note glides *and* re-hits, otherwise it'd be silent.
        if (s.mode == ModeLegato && overlapping && v.ampLevel() >= 0.35f)
            v.slideTo (note);
        else
        {
            float from = -1.0f;
            if (v.isActive() && (s.mode == ModeMono || overlapping))
                from = v.pitchNow();
            // Retriggering a voice that's still sounding would restart its waveform mid-cycle (a click):
            // fade it out and start the new note on a fresh voice instead.
            Voice* target = &v;
            if (v.isActive())
            {
                v.fadeOut();
                target = &allocateVoice();
                monoVoice = (int) (target - voices.data());
            }
            target->start (note, velocity, from, true, s, globalMod, noteCounter);
            target->trigger = note;
        }
    }
    if (s.mode != ModePoly) voices[(size_t) monoVoice].trigger = voices[(size_t) monoVoice].note;
    lastNote = note;
}

Voice& HypernovaAudioProcessor::allocateVoice()
{
    // Over the polyphony limit: fade the oldest playing voice (released ones first) instead of cutting it.
    int playing = 0;
    for (auto& v : voices) if (v.isActive() && ! v.fading) ++playing;
    if (playing >= PolyLimit)
    {
        Voice* victim = nullptr;
        for (auto& v : voices)
            if (v.isActive() && ! v.fading && ! v.held && (victim == nullptr || v.age < victim->age)) victim = &v;
        if (victim == nullptr)
            for (auto& v : voices)
                if (v.isActive() && ! v.fading && (victim == nullptr || v.age < victim->age)) victim = &v;
        if (victim != nullptr) victim->fadeOut();
    }
    // A silent slot if there is one; otherwise the fading voice closest to silence.
    for (auto& v : voices)
        if (! v.isActive()) return v;
    Voice* target = nullptr;
    for (auto& v : voices)
        if (target == nullptr || v.ampLevel() < target->ampLevel()) target = &v;
    return *target;
}

void HypernovaAudioProcessor::noteOff (int note, int channel)
{
    juce::ignoreUnused (channel);
    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
    if (blockSettings.mode == ModePoly)
    {
        for (size_t i = 0; i < voices.size(); ++i)
        {
            auto& v = voices[i];
            if (v.trigger == note && v.held)
            {
                if (sustainPedal) { v.held = false; sustained[i] = true; }
                else v.stop (! blockSettings.smp.chopHold);
            }
        }
        return;
    }

    auto& v = voices[(size_t) monoVoice];
    if (v.note != note || ! v.held) return;
    if (! heldNotes.empty())
    {
        v.slideTo (heldNotes.back());
        lastNote = heldNotes.back();
    }
    else if (sustainPedal)
    {
        v.held = false;
        sustained[(size_t) monoVoice] = true;
    }
    else
        v.stop (! blockSettings.smp.chopHold);
}

void HypernovaAudioProcessor::allNotesOff (bool hard)
{
    heldNotes.clear();
    for (size_t i = 0; i < voices.size(); ++i)
    {
        sustained[i] = false;
        if (hard) voices[i].reset();
        else voices[i].stop();
    }
}

// Quality: the whole voice engine runs at 1x (Eco), 2x (High) or 4x (Ultra) the host rate, then a steep
// half-band filter brings it back down. Warps, FM, sync, filter drive and the sub's edges stop aliasing.
// Only patches that can alias get oversampled: plain wavetables are already band-limited, so they stay at 1x
// and cost nothing extra. Warps, FM, sync and filter drive switch to 2x (High) or 4x (Ultra).
static bool patchCanAlias (const SynthSettings& s)
{
    for (const auto& o : s.osc)
        if (o.on && o.warp != WarpOff) return true;
    if (s.filterOn && (s.filterDrive > 0.001f || s.filterType == FDirty)) return true;
    // Cross modulation makes sidebands well above the table's own band limit.
    if (s.xFmAB > 0.001f || s.xFmBA > 0.001f || s.xRing > 0.001f || s.xAm > 0.001f || s.xFltFm > 0.001f) return true;
    for (const auto& m : s.mod)
        if (m.src != SrcNone && (m.dest == DAWarp || m.dest == DBWarp || m.dest == DDrive)) return true;
    return false;
}

void HypernovaAudioProcessor::applyQuality (int q)
{
    q = juce::jlimit (0, 2, q);
    const int want = q == 0 || ! patchCanAlias (blockSettings) ? 1 : (q == 1 ? 2 : 4);
    if (want == osFactor && currentQuality >= 0) return;
    // Changing rate resets voices, so wait until nothing is sounding (preset changes are silent anyway).
    bool sounding = false;
    for (auto& v : voices) sounding = sounding || v.isActive();
    if (sounding && currentQuality >= 0) return;
    currentQuality = q;
    osFactor = want;
    allNotesOff (true);
    for (auto& v : voices) v.prepare (sampleRateNow * osFactor);
    auto* os = osFactor > 1 ? voiceOversampler[(size_t) (osFactor == 2 ? 0 : 1)].get() : nullptr;
    if (os != nullptr) os->reset();
    setLatencySamples ((os != nullptr ? juce::roundToInt (os->getLatencyInSamples()) : 0) + limiterLen);
}

void HypernovaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    // Hosts may send more than they announced; run in chunks we're prepared for.
    const int total = buffer.getNumSamples();
    if (total <= maxBlock) { processChunk (buffer, midi); return; }
    juce::MidiBuffer part;
    for (int start = 0; start < total; start += maxBlock)
    {
        const int n = juce::jmin (maxBlock, total - start);
        part.clear();
        for (const auto meta : midi)
            if (meta.samplePosition >= start && meta.samplePosition < start + n)
                part.addEvent (meta.getMessage(), meta.samplePosition - start);
        float* chans[2] = { buffer.getWritePointer (0, start), buffer.getWritePointer (1, start) };
        juce::AudioBuffer<float> sub (chans, 2, n);
        processChunk (sub, part);
    }
    midi.clear();
}

void HypernovaAudioProcessor::processChunk (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    // Asleep: nothing playing and every effect tail has died away. Stay asleep (zero CPU) until MIDI arrives.
    if (sleeping)
    {
        bool anyMidi = false;
        for (const auto meta : midi) { juce::ignoreUnused (meta); anyMidi = true; break; }
        if (! anyMidi && ! panicRequested.load()) return;
        sleeping = false;
        silentSamples = 0;
    }

    if (panicRequested.exchange (false))
    {
        allNotesOff (true);
        effects.reset();
    }

    // Tempo and host-locked LFO phase.
    double ppq = -1.0;
    bool playing = false;
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = (float) *b;
            if (auto p = pos->getPpqPosition()) ppq = *p;
            playing = pos->getIsPlaying();
        }
    hostPpq = ppq >= 0.0 ? ppq : 0.0;
    hostPlaying = playing && ppq >= 0.0;
    freeBeats += buffer.getNumSamples() * bpm / 60.0 / juce::jmax (8000.0, sampleRateNow);
    shownBeats = hostPlaying ? hostPpq : freeBeats;
    shownBpm = bpm;

    updateOrbit (numSamples);   // the blend is worked out first: every setting below is read through it
    blockSettings = readSynthSettings();
    applyQuality ((int) param ("quality"));
    smoothSettings (blockSettings, numSamples);
    auto& settings = blockSettings;
    if (settings.mode != lastMode)
    {
        if (lastMode >= 0) allNotesOff (false);
        lastMode = settings.mode;
    }

    for (int i = 0; i < NumLfo; ++i)
    {
        const int sync = (int) param (("lfo" + std::to_string (i + 1) + "Sync").c_str());
        if (sync != 0 && playing && ppq >= 0.0)
            freeLfoPhase[i] = dsp::frac (ppq / lfoSyncBeats (sync));
        globalMod.lfoPhase[(size_t) i] = freeLfoPhase[i];
        freeLfoPhase[i] = dsp::frac (freeLfoPhase[i] + settings.lfo[(size_t) i].rateHz * numSamples / sampleRateNow);
    }
    for (int m = 0; m < 4; ++m)
        globalMod.macros[(size_t) m] = param (("macro" + std::to_string (m + 1)).c_str());

    runArpeggiator (midi, numSamples, playing ? ppq : -1.0, playing);

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    // Voices render into the oversampled bus (or straight into the output in Eco).
    juce::dsp::AudioBlock<float> outBlock (buffer);
    juce::dsp::Oversampling<float>* os = osFactor > 1 ? voiceOversampler[(size_t) (osFactor == 2 ? 0 : 1)].get() : nullptr;
    float* vL = L;
    float* vR = R;
    if (os != nullptr)
    {
        auto up = os->processSamplesUp (outBlock);
        up.clear();
        vL = up.getChannelPointer (0);
        vR = up.getChannelPointer (1);
    }

    // The alt bus only costs anything when a source is actually playing into it.
    const bool useAlt = altBusInUse (settings);
    float* vaL = nullptr;
    float* vaR = nullptr;
    juce::dsp::AudioBlock<float> altBlock;
    juce::dsp::Oversampling<float>* osAlt = nullptr;
    if (useAlt)
    {
        if (altBuf.getNumSamples() < numSamples) altBuf.setSize (2, numSamples, false, false, true);
        altBuf.clear (0, numSamples);
        altBlock = juce::dsp::AudioBlock<float> (altBuf).getSubBlock (0, (size_t) numSamples);
        osAlt = osFactor > 1 ? altOversampler[(size_t) (osFactor == 2 ? 0 : 1)].get() : nullptr;
        if (osAlt != nullptr)
        {
            auto up = osAlt->processSamplesUp (altBlock);
            up.clear();
            vaL = up.getChannelPointer (0);
            vaR = up.getChannelPointer (1);
        }
        else
        {
            vaL = altBuf.getWritePointer (0);
            vaR = altBuf.getWritePointer (1);
        }
    }
    const int f = osFactor;
    auto renderVoices = [&] (int from, int to)
    {
        if (to <= from) return;
        for (auto& v : voices)
            v.render (vL + from * f, vR + from * f, (to - from) * f, settings, globalMod,
                      vaL != nullptr ? vaL + from * f : nullptr, vaR != nullptr ? vaR + from * f : nullptr);
    };

    // Hosts often send the next note-on before the previous note-off at the same sample. Handle offs first
    // so back-to-back notes re-attack on time instead of sliding silently or cutting the new note.
    midiOrder.clear();
    for (const auto meta : midi)
        midiOrder.push_back (meta);
    std::stable_sort (midiOrder.begin(), midiOrder.end(), [] (const juce::MidiMessageMetadata& a, const juce::MidiMessageMetadata& b)
    {
        if (a.samplePosition != b.samplePosition) return a.samplePosition < b.samplePosition;
        return ! a.getMessage().isNoteOn() && b.getMessage().isNoteOn();
    });

    int cursor = 0;
    for (const auto& meta : midiOrder)
    {
        const auto msg = meta.getMessage();
        const int pos = juce::jlimit (0, numSamples, meta.samplePosition);
        renderVoices (cursor, pos);
        cursor = pos;

        const int ch = juce::jlimit (1, 16, msg.getChannel());
        if (msg.isNoteOn())
            noteOn (msg.getNoteNumber(), msg.getFloatVelocity(), ch);
        else if (msg.isNoteOff())
            noteOff (msg.getNoteNumber(), ch);
        else if (msg.isPitchWheel())
        {
            // With MPE on, a bend on a note's own channel bends that note alone; channel 1 stays the master.
            if (mpeOn() && ch != 1)
            {
                channelExpression[(size_t) ch].bend = (float) (msg.getPitchWheelValue() - 8192) / 8192.0f * param ("mpeBend");
                applyExpressionToVoices (ch);
            }
            else pitchWheel = msg.getPitchWheelValue();
        }
        else if (msg.isChannelPressure() || msg.isAftertouch())
        {
            // Pressure: per channel with MPE, per note with poly aftertouch, otherwise it moves every voice.
            const float v = (float) (msg.isAftertouch() ? msg.getAfterTouchValue() : msg.getChannelPressureValue()) / 127.0f;
            if (msg.isAftertouch())
            {
                for (auto& voice : voices)
                    if (voice.isActive() && voice.note == msg.getNoteNumber() && (! mpeOn() || voice.channel == ch)) voice.pressure = v;
            }
            else
            {
                channelExpression[(size_t) ch].pressure = v;
                applyExpressionToVoices (ch);
            }
        }
        else if (msg.isController())
        {
            if (msg.getControllerNumber() == 74)
            {
                // Slide (timbre): the third MPE dimension, centred.
                channelExpression[(size_t) ch].slide = (float) msg.getControllerValue() / 127.0f * 2.0f - 1.0f;
                applyExpressionToVoices (ch);
            }
            else if (msg.getControllerNumber() == 1)
                globalMod.modWheel = (float) msg.getControllerValue() / 127.0f;
            else if (msg.getControllerNumber() == 64)
            {
                sustainPedal = msg.getControllerValue() >= 64;
                if (! sustainPedal)
                    for (size_t i = 0; i < voices.size(); ++i)
                        if (sustained[i]) { sustained[i] = false; if (! voices[i].held) voices[i].stop (! blockSettings.smp.chopHold); }
            }
            else if (msg.getControllerNumber() == 123 || msg.getControllerNumber() == 120)
                allNotesOff (msg.getControllerNumber() == 120);
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            allNotesOff (msg.isAllSoundOff());

        globalMod.bendSemis = (float) (pitchWheel - 8192) / 8192.0f * param ("bendRange");
    }
    globalMod.bendSemis = (float) (pitchWheel - 8192) / 8192.0f * param ("bendRange");
    renderVoices (cursor, numSamples);
    midi.clear();
    if (os != nullptr) os->processSamplesDown (outBlock);
    if (useAlt && osAlt != nullptr) osAlt->processSamplesDown (altBlock);
    // The follower listens to everything the voices played, whichever bus it went to.
    updateFollower (L, R, numSamples, useAlt ? altBuf.getReadPointer (0) : nullptr, useAlt ? altBuf.getReadPointer (1) : nullptr);

    // Master effects in chunks the effects were prepared for.
    auto fx = readFxSettings();
    applyGlobalModulation (fx);
    for (int start = 0; start < numSamples; start += maxBlock)
    {
        const int n = juce::jmin (maxBlock, numSamples - start);
        float* chans[2] = { L + start, R + start };
        juce::AudioBuffer<float> chunk (chans, 2, n);
        if (useAlt)
        {
            float* altChans[2] = { altBuf.getWritePointer (0) + start, altBuf.getWritePointer (1) + start };
            juce::AudioBuffer<float> altChunk (altChans, 2, n);
            effects.process (chunk, &altChunk, fx);
        }
        else effects.process (chunk, fx);
    }
    {
        // Which slice the newest voice is playing, so the waveform can light it up.
        int slice = -1;
        const Voice* newest = nullptr;
        for (auto& v : voices)
            if (v.isActive() && (newest == nullptr || v.age > newest->age)) newest = &v;
        if (newest != nullptr) slice = newest->playingSlice();
        shownSlice.store (slice, std::memory_order_relaxed);
    }
    shownLowRms = fx.lowOn ? effects.lastLowRms : 0.0f;
    shownHighRms = fx.lowOn ? effects.lastHighRms : 0.0f;

    // Output: master volume, then a transparent look-ahead peak limiter at -0.3 dBFS. The required gain is
    // min-held over the look-ahead window and box-car smoothed across it, so the gain glides down over ~1.5 ms
    // and reaches each peak's target exactly as that peak leaves the delay line; recovery takes ~80 ms.
    masterGain.setTargetValue (juce::Decibels::decibelsToGain (param ("volume")));
    const float ceiling = 0.966f;
    const float release = std::exp (-1.0f / (0.08f * (float) sampleRateNow));
    const int N = limiterLen;
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = masterGain.getNextValue();
        float l = L[i] * g, r = R[i] * g;
        if (! std::isfinite (l)) l = 0.0f;
        if (! std::isfinite (r)) r = 0.0f;
        const float peak = juce::jmax (std::abs (l), std::abs (r));
        const float target = peak > ceiling ? ceiling / peak : 1.0f;

        const size_t p = (size_t) limiterPos;
        limiterLoud += (target < 1.0f ? 1 : 0) - (limTarget[p] < 1.0f ? 1 : 0);
        limTarget[p] = target;
        float held = 1.0f;
        if (limiterLoud > 0)
            for (int k = 0; k < N; ++k) held = juce::jmin (held, limTarget[(size_t) k]);
        // release only ever raises the held gain slowly; drops pass straight through to the smoother
        limiterGain = held < limiterGain ? held : held + release * (limiterGain - held);
        limiterSum += (double) limiterGain - (double) limHeld[p];
        limHeld[p] = limiterGain;
        // The sample leaving the delay (written N samples ago) is covered by the box-car as it stood one
        // sample ago, whose whole window was min-held over that sample's target.
        const float gain = limiterOutGain;
        limiterOutGain = juce::jmin (1.0f, (float) (limiterSum / N));

        const float outL = limDelayL[p], outR = limDelayR[p];
        limDelayL[p] = l;
        limDelayR[p] = r;
        L[i] = juce::jlimit (-ceiling, ceiling, outL * gain);
        R[i] = juce::jlimit (-ceiling, ceiling, outR * gain);
        limiterPos = limiterPos + 1 < N ? limiterPos + 1 : 0;
    }
    limiterSum = 0; // re-sum once per block so float round-off can't creep into the box-car
    for (auto v : limHeld) limiterSum += v;

    scope.push (L, R, numSamples);

    // Phone-speaker check: monitoring only (never saved, never in a bounce unless you leave it on).
    if (speakerCheck.load (std::memory_order_relaxed))
        for (int c = 0; c < 2; ++c)
        {
            float* d = c == 0 ? L : R;
            for (int i = 0; i < numSamples; ++i)
                d[i] = speakerLp[c].processSample (speakerBump[c].processSample (speakerHp2[c].processSample (speakerHp[c].processSample (d[i]))));
        }

    // Fall asleep after a second of true silence with no voices (tails included).
    {
        bool anyVoice = false;
        for (auto& v : voices) anyVoice = anyVoice || v.isActive();
        const float peak = juce::jmax (buffer.getMagnitude (0, 0, numSamples), buffer.getMagnitude (1, 0, numSamples));
        if (! anyVoice && heldNotes.empty() && arpKeys.empty() && peak < 1.0e-5f)
        {
            silentSamples += numSamples;
            if (silentSamples > (int) sampleRateNow) { sleeping = true; effects.reset(); }
        }
        else silentSamples = 0;
    }

    // Visualiser taps from the newest sounding voice.
    const Voice* newest = nullptr;
    int active = 0;
    for (auto& v : voices)
        if (v.isActive())
        {
            ++active;
            if (newest == nullptr || v.age > newest->age) newest = &v;
        }
    shownVoices = active;
    if (newest != nullptr)
    {
        for (int o = 0; o < NumOsc; ++o) shownPos[o] = newest->shownPos[o];
        for (int o = 0; o < NumLfo; ++o)
        {
            shownLfo[o] = newest->shownLfo[o];
            shownLfoPhase[o] = (float) newest->shownLfoPhase[o];
        }
        shownEnv = newest->ampLevel();
        shownCutoff = newest->shownCutoff;
        shownNote = newest->note;
        shownSample = newest->shownSample;
        publishModSources (newest->shownLfo, newest->shownModEnv, newest->shownVelocity, (float) newest->note);
    }
    else
    {
        for (int o = 0; o < NumOsc; ++o) shownPos[o] = settings.osc[(size_t) o].pos;
        float idle[NumLfo];
        for (int o = 0; o < NumLfo; ++o)
        {
            shownLfoPhase[o] = (float) globalMod.lfoPhase[(size_t) o];
            idle[o] = dsp::lfoShape (settings.lfo[(size_t) o].shape, globalMod.lfoPhase[(size_t) o], 0, 0, settings.lfo[(size_t) o].table);
            shownLfo[o] = idle[o];
        }
        shownEnv = 0;
        shownCutoff = settings.cutoff;
        shownNote = -1;
        shownSample = -1.0f;
        publishModSources (idle, 0.0f, 0.0f, -1.0f);
    }
}

// Feeds the editor the live value of every modulation source, so mod rings can animate.
void HypernovaAudioProcessor::publishModSources (const float* lfo, float modEnv, float velocity, float note)
{
    shownModSource[SrcLfo1] = lfo[0];
    shownModSource[SrcLfo2] = lfo[1];
    shownModSource[SrcLfo3] = lfo[2];
    shownModSource[SrcLfo4] = lfo[3];
    shownModSource[3] = modEnv;
    shownModSource[4] = velocity;
    shownModSource[5] = globalMod.modWheel;
    shownModSource[6] = note >= 0 ? juce::jlimit (-1.0f, 1.0f, (note - 60.0f) / 36.0f) : 0.0f;
    for (int m = 0; m < 4; ++m) shownModSource[(size_t) (7 + m)] = globalMod.macros[(size_t) m];
    shownModSource[SrcFollow] = globalMod.follower;
    {
        // Pressure and slide: shown from the newest voice that's playing.
        const Voice* newest = nullptr;
        for (auto& v : voices) if (v.isActive() && (newest == nullptr || v.age > newest->age)) newest = &v;
        shownModSource[SrcPressure] = newest != nullptr ? newest->pressure : 0.0f;
        shownModSource[SrcSlide] = newest != nullptr ? newest->slide : 0.0f;
    }
    shownModSource[11] = 0.0f; // random is per note; its ring shows depth only
}

// Hands a channel's expression to the notes playing on it (all of them when MPE is off, so plain
// aftertouch and CC 74 still work as modulation sources).
void HypernovaAudioProcessor::applyExpressionToVoices (int channel)
{
    const auto& e = channelExpression[(size_t) juce::jlimit (1, 16, channel)];
    const bool mpe = mpeOn();
    for (auto& v : voices)
    {
        if (! v.isActive()) continue;
        if (mpe && v.channel != channel) continue;
        if (mpe) v.noteBend = e.bend;
        v.pressure = e.pressure;
        v.slide = e.slide;
    }
}

// The envelope follower: how loud the synth is right now, smoothed with its own attack and release.
// It's taken from the voices (before the effects), so feeding it back into them can't run away.
// ---------------------------------------------------------------------------------------------------
// Chop Lab: the sample cut into slices, one per key from the chop root up. The edges are kept with the
// sound as a list of positions; the audio thread reads a copy that is swapped in whole.
void HypernovaAudioProcessor::setSlices (const juce::String& text)
{
    ab::Slices next;
    juce::StringArray parts;
    parts.addTokens (text, " ", "");
    std::vector<float> at;
    for (const auto& piece : parts)
    {
        if (piece.trim().isEmpty()) continue;
        at.push_back (juce::jlimit (0.0f, 1.0f, piece.getFloatValue()));
    }
    std::sort (at.begin(), at.end());
    // The edges always run from the start of the sample to its end, and never two in the same place.
    std::vector<float> edges { 0.0f };
    for (float v : at)
        if (v > edges.back() + 0.002f && v < 0.998f) edges.push_back (v);
    edges.push_back (1.0f);
    if ((int) edges.size() > ab::MaxSlices + 1) edges.resize ((size_t) ab::MaxSlices + 1);
    if (edges.size() >= 3)   // fewer than that is not a chop at all
    {
        next.edges = (int) edges.size();
        for (size_t i = 0; i < edges.size(); ++i) next.at[i] = edges[i];
    }
    const int side = 1 - sliceSide.load (std::memory_order_acquire);
    sliceTable[(size_t) side] = next;
    sliceSide.store (side, std::memory_order_release);

    juce::String saved;
    for (int i = 0; i < next.edges; ++i) saved << juce::String (next.at[(size_t) i], 5) << " ";
    apvts.state.setProperty ("slices", saved.trim(), &undoManager);
    ++presetVersion;
}

juce::String HypernovaAudioProcessor::sliceText() const
{
    return apvts.state.getProperty ("slices", "").toString();
}

ab::Slices HypernovaAudioProcessor::slicesForUi() const
{
    return sliceTable[(size_t) sliceSide.load (std::memory_order_acquire)];
}

void HypernovaAudioProcessor::readSlicesFromState()
{
    setSlices (apvts.state.getProperty ("slices", "").toString());
}

// Cut the sample up: either where its hits are, or into equal pieces.
void HypernovaAudioProcessor::chopSample (int slices)
{
    const auto* data = currentSample.load (std::memory_order_acquire);
    if (data == nullptr || data->length < 64) return;
    undoManager.beginNewTransaction (slices > 0 ? "Chop into " + juce::String (slices) : juce::String ("Chop on the hits"));
    juce::String text;
    if (slices > 0)
    {
        for (int i = 0; i <= slices; ++i) text << juce::String ((float) i / (float) slices, 5) << " ";
    }
    else
    {
        std::vector<float> mono ((size_t) data->length);
        for (int i = 0; i < data->length; ++i) mono[(size_t) i] = 0.5f * (data->l[(size_t) i + 2] + data->r[(size_t) i + 2]);
        for (float at : ab::dsp::findOnsets (mono, data->rate, ab::MaxSlices)) text << juce::String (at, 5) << " ";
        text << "1";
    }
    setSlices (text.trim());
    setParam ("chopOn", 1.0f);
    apvts.copyState();
}

// ---------------------------------------------------------------------------------------------------
// Orbit: four captured sounds, and a point between them. Worked out once a block, before anything reads
// a parameter, so the whole engine hears the blend without knowing anything about it.
void HypernovaAudioProcessor::updateOrbit (int numSamples)
{
    // Orbit's own controls are read straight from the knobs: it never morphs itself.
    auto plain = [this] (const char* id)
    {
        auto it = raw.find (id);
        return it != raw.end() ? rawValue[(size_t) it->second]->load (std::memory_order_relaxed) : 0.0f;
    };
    const auto& corners = orbitCorners[(size_t) orbitSide.load (std::memory_order_acquire)];
    if (plain ("orbitOn") < 0.5f || corners.howMany() == 0 || morphed.size() != rawValue.size())
    {
        morphActive.store (false, std::memory_order_relaxed);
        return;
    }

    float x = plain ("orbitX"), y = plain ("orbitY");

    // Anything in the matrix aimed at the morph moves it too. These rows are read from the matrix as it is
    // being edited, not from the blend: a captured sound's own matrix must not take the morph over.
    {
        const Voice* newest = nullptr;
        for (auto& v : voices)
            if (v.isActive() && (newest == nullptr || v.age > newest->age)) newest = &v;
        for (int i = 0; i < NumModSlots; ++i)
        {
            const std::string row = "mod" + std::to_string (i + 1);
            const int dest = (int) plain ((row + "Dest").c_str());
            if (dest != DMorphX && dest != DMorphY) continue;
            const int src = (int) plain ((row + "Src").c_str());
            if (src == SrcNone || src >= NumSrc) continue;
            float v = 0;
            if (src >= SrcMacro1 && src <= SrcMacro4) v = globalMod.macros[(size_t) (src - SrcMacro1)];
            else if (src == SrcModWheel) v = globalMod.modWheel;
            else if (src == SrcFollow) v = globalMod.follower;
            else if (newest != nullptr) v = newest->lastSrc[src];
            (dest == DMorphX ? x : y) += shapeMod ((int) plain ((row + "Shape").c_str()), v) * plain ((row + "Amt").c_str());
        }
    }

    // A path of its own: the point keeps travelling around wherever it was left.
    const int path = (int) plain ("orbitPath");
    const float depth = plain ("orbitDepth");
    if (path != ab::Orbit::Still && depth > 0.001f)
    {
        const int sync = (int) plain ("orbitSync");
        const double hz = sync == 0 ? (double) plain ("orbitRate") : bpm / 60.0 / lfoSyncBeats (sync);
        if (sync != 0 && hostPlaying) orbitPhase = hostPpq / (lfoSyncBeats (sync) * 4.0);
        else orbitPhase += hz * numSamples / juce::jmax (8000.0, sampleRateNow);
        orbitPhase -= std::floor (orbitPhase);
        const auto off = orbitTravel.step (path, orbitPhase, (float) numSamples / (float) juce::jmax (8000.0, sampleRateNow));
        x += off.x * depth * 0.5f;
        y += off.y * depth * 0.5f;
    }

    // Smoothed over about 15 ms, so jumping across the square slides instead of stepping.
    const float k = juce::jlimit (0.0f, 1.0f, (float) numSamples / (0.015f * (float) juce::jmax (8000.0, sampleRateNow)));
    morphNowX += (juce::jlimit (0.0f, 1.0f, x) - morphNowX) * k;
    morphNowY += (juce::jlimit (0.0f, 1.0f, y) - morphNowY) * k;

    const auto w = ab::Orbit::weights (morphNowX, morphNowY, corners.filled);
    const int nearest = ab::Orbit::strongest (w);
    const auto& near = corners.values[(size_t) nearest];
    if (near.size() != rawValue.size()) { morphActive.store (false, std::memory_order_relaxed); return; }

    for (size_t i = 0; i < rawValue.size(); ++i)
    {
        if (rawMorphs[i] == 0) continue;
        if (rawDiscrete[i] != 0) { morphed[i].store (near[i], std::memory_order_relaxed); continue; }
        float v = 0;
        for (int c = 0; c < ab::Orbit::NumCorners; ++c)
            if (w[(size_t) c] > 0.0f && corners.values[(size_t) c].size() == rawValue.size())
                v += w[(size_t) c] * corners.values[(size_t) c][i];
        morphed[i].store (v, std::memory_order_relaxed);
    }
    morphActive.store (true, std::memory_order_relaxed);
    morphSeen.store (true, std::memory_order_relaxed);
    shownMorphX.store (morphNowX, std::memory_order_relaxed);
    shownMorphY.store (morphNowY, std::memory_order_relaxed);
    for (int c = 0; c < ab::Orbit::NumCorners; ++c) shownWeights[(size_t) c].store (w[(size_t) c], std::memory_order_relaxed);
}

// Live means Orbit is switched on with something captured: the same answer whether or not audio is running,
// so the display never flickers while the plugin is asleep.
bool HypernovaAudioProcessor::orbitLive() const
{
    auto it = raw.find ("orbitOn");
    if (it == raw.end() || rawValue[(size_t) it->second]->load() < 0.5f) return false;
    return cornersFilled() > 0;
}

float HypernovaAudioProcessor::soundValue (const juce::String& id) const
{
    return param (id.toRawUTF8());
}

juce::Point<float> HypernovaAudioProcessor::orbitPoint() const
{
    // Before any audio has run (or with Orbit off) the point is simply where the controls put it.
    if (! morphSeen.load (std::memory_order_relaxed) || ! orbitLive())
    {
        auto at = [this] (const char* id)
        {
            auto it = raw.find (id);
            return it != raw.end() ? rawValue[(size_t) it->second]->load() : 0.0f;
        };
        return { at ("orbitX"), at ("orbitY") };
    }
    return { shownMorphX.load (std::memory_order_relaxed), shownMorphY.load (std::memory_order_relaxed) };
}

std::array<float, ab::Orbit::NumCorners> HypernovaAudioProcessor::orbitWeights() const
{
    std::array<float, ab::Orbit::NumCorners> w {};
    for (int c = 0; c < ab::Orbit::NumCorners; ++c) w[(size_t) c] = shownWeights[(size_t) c].load (std::memory_order_relaxed);
    return w;
}

bool HypernovaAudioProcessor::cornerFilled (int corner) const
{
    if (! juce::isPositiveAndBelow (corner, ab::Orbit::NumCorners)) return false;
    return orbitCorners[(size_t) orbitSide.load (std::memory_order_acquire)].filled[(size_t) corner];
}

int HypernovaAudioProcessor::cornersFilled() const
{
    return orbitCorners[(size_t) orbitSide.load (std::memory_order_acquire)].howMany();
}

juce::String HypernovaAudioProcessor::cornerName (int corner) const
{
    if (! juce::isPositiveAndBelow (corner, ab::Orbit::NumCorners)) return {};
    auto tree = apvts.state.getChildWithName ("orbit").getChild (corner);
    return tree.isValid() ? tree.getProperty ("name", "").toString() : juce::String();
}

// The captured sounds are kept in the state tree, so they travel with the sound, with presets, and with A/B.
// Only what differs from a parameter's default is written, so a corner holding an Init sound costs nothing.
void HypernovaAudioProcessor::writeCornersToState (const ab::Orbit::Corners& c)
{
    auto orbit = apvts.state.getOrCreateChildWithName ("orbit", &undoManager);
    while (orbit.getNumChildren() < ab::Orbit::NumCorners) orbit.appendChild (juce::ValueTree ("corner"), &undoManager);
    for (int i = 0; i < ab::Orbit::NumCorners; ++i)
    {
        auto tree = orbit.getChild (i);
        if (! c.filled[(size_t) i] || c.values[(size_t) i].size() != rawValue.size())
        {
            tree.setProperty ("filled", false, &undoManager);
            tree.setProperty ("data", "", &undoManager);
            continue;
        }
        juce::String data;
        for (size_t k = 0; k < rawValue.size(); ++k)
        {
            if (rawMorphs[k] == 0) continue;
            const float v = c.values[(size_t) i][k];
            if (std::abs (v - rawParam[k]->convertFrom0to1 (rawParam[k]->getDefaultValue())) < 1.0e-6f) continue;
            data << rawParam[k]->getParameterID() << "=" << juce::String (v, 6) << " ";
        }
        tree.setProperty ("filled", true, &undoManager);
        tree.setProperty ("data", data.trim(), &undoManager);
    }
}

void HypernovaAudioProcessor::readCornersFromState()
{
    ab::Orbit::Corners next;
    auto orbit = apvts.state.getChildWithName ("orbit");
    for (int i = 0; i < ab::Orbit::NumCorners; ++i)
    {
        auto tree = orbit.isValid() ? orbit.getChild (i) : juce::ValueTree();
        if (! tree.isValid() || ! (bool) tree.getProperty ("filled", false)) continue;
        auto& values = next.values[(size_t) i];
        values.resize (rawValue.size());
        for (size_t k = 0; k < rawValue.size(); ++k) values[k] = rawParam[k]->convertFrom0to1 (rawParam[k]->getDefaultValue());
        juce::StringArray pairs;
        pairs.addTokens (tree.getProperty ("data", "").toString(), " ", "");
        for (const auto& pair : pairs)
        {
            const int eq = pair.indexOfChar ('=');
            if (eq <= 0) continue;
            auto it = raw.find (pair.substring (0, eq).toStdString());
            if (it != raw.end()) values[(size_t) it->second] = pair.substring (eq + 1).getFloatValue();
        }
        next.filled[(size_t) i] = true;
    }
    const int side = 1 - orbitSide.load (std::memory_order_acquire);
    orbitCorners[(size_t) side] = std::move (next);
    orbitSide.store (side, std::memory_order_release);
}

void HypernovaAudioProcessor::captureCorner (int corner)
{
    if (! juce::isPositiveAndBelow (corner, ab::Orbit::NumCorners)) return;
    undoManager.beginNewTransaction ("Capture " + ab::Orbit::cornerNames()[corner]);
    auto next = orbitCorners[(size_t) orbitSide.load (std::memory_order_acquire)];
    auto& values = next.values[(size_t) corner];
    values.resize (rawValue.size());
    // The sound as it is now, which with Orbit live is the blend you are hearing.
    for (size_t k = 0; k < rawValue.size(); ++k)
        values[k] = morphActive.load() && rawMorphs[k] != 0 ? morphed[k].load() : rawValue[k]->load();
    next.filled[(size_t) corner] = true;
    writeCornersToState (next);
    {
        const juce::ScopedLock sl (nameLock);
        apvts.state.getChildWithName ("orbit").getChild (corner).setProperty ("name", presetName, &undoManager);
    }
    const int side = 1 - orbitSide.load (std::memory_order_acquire);
    orbitCorners[(size_t) side] = std::move (next);
    orbitSide.store (side, std::memory_order_release);
    apvts.copyState();
}

void HypernovaAudioProcessor::putPresetInCorner (int corner, int program)
{
    if (! juce::isPositiveAndBelow (corner, ab::Orbit::NumCorners)) return;
    if (! juce::isPositiveAndBelow (program, (int) ab::factoryPresets().size())) return;
    undoManager.beginNewTransaction ("Put a sound in " + ab::Orbit::cornerNames()[corner]);
    // Work out what that preset's parameters would be, without touching the sound being played.
    std::vector<float> values ((size_t) rawValue.size());
    for (size_t k = 0; k < rawValue.size(); ++k) values[k] = rawParam[k]->convertFrom0to1 (rawParam[k]->getDefaultValue());
    for (const auto& [id, value] : ab::factoryPresets()[(size_t) program].values)
    {
        auto it = raw.find (id);
        if (it != raw.end()) values[(size_t) it->second] = value;
    }
    auto next = orbitCorners[(size_t) orbitSide.load (std::memory_order_acquire)];
    next.values[(size_t) corner] = std::move (values);
    next.filled[(size_t) corner] = true;
    writeCornersToState (next);
    apvts.state.getChildWithName ("orbit").getChild (corner)
        .setProperty ("name", juce::String (ab::factoryPresets()[(size_t) program].name), &undoManager);
    const int side = 1 - orbitSide.load (std::memory_order_acquire);
    orbitCorners[(size_t) side] = std::move (next);
    orbitSide.store (side, std::memory_order_release);
    apvts.copyState();
}

void HypernovaAudioProcessor::clearCorner (int corner)
{
    if (! juce::isPositiveAndBelow (corner, ab::Orbit::NumCorners)) return;
    undoManager.beginNewTransaction ("Empty " + ab::Orbit::cornerNames()[corner]);
    auto next = orbitCorners[(size_t) orbitSide.load (std::memory_order_acquire)];
    next.filled[(size_t) corner] = false;
    next.values[(size_t) corner].clear();
    writeCornersToState (next);
    apvts.state.getChildWithName ("orbit").getChild (corner).setProperty ("name", "", &undoManager);
    const int side = 1 - orbitSide.load (std::memory_order_acquire);
    orbitCorners[(size_t) side] = std::move (next);
    orbitSide.store (side, std::memory_order_release);
    apvts.copyState();
}

// Keep what you are hearing: the blend is written into the knobs and Orbit switches off.
void HypernovaAudioProcessor::bakeOrbit()
{
    if (! morphActive.load()) return;
    undoManager.beginNewTransaction ("Keep the blend");
    std::vector<float> values ((size_t) rawValue.size());
    for (size_t k = 0; k < rawValue.size(); ++k) values[k] = rawMorphs[k] != 0 ? morphed[k].load() : rawValue[k]->load();
    morphActive.store (false, std::memory_order_relaxed);
    for (size_t k = 0; k < rawValue.size(); ++k)
        if (rawMorphs[k] != 0 && std::abs (values[k] - rawValue[k]->load()) > 1.0e-6f)
            rawParam[k]->setValueNotifyingHost (rawParam[k]->convertTo0to1 (values[k]));
    setParam ("orbitOn", 0.0f);
    apvts.copyState();
}

void HypernovaAudioProcessor::updateFollower (const float* L, const float* R, int n, const float* altL, const float* altR)
{
    const float att = juce::jmax (0.5f, param ("folAtt")) * 0.001f;
    const float rel = juce::jmax (1.0f, param ("folRel")) * 0.001f;
    const float gain = param ("folGain");
    const float aC = std::exp (-1.0f / juce::jmax (1.0f, (float) (att * sampleRateNow)));
    const float rC = std::exp (-1.0f / juce::jmax (1.0f, (float) (rel * sampleRateNow)));
    float env = followerEnv;
    for (int i = 0; i < n; ++i)
    {
        float x = juce::jmax (std::abs (L[i]), std::abs (R[i]));
        if (altL != nullptr) x = juce::jmax (x, juce::jmax (std::abs (altL[i]), std::abs (altR[i])));
        x *= gain;
        env = x > env ? x + (env - x) * aC : x + (env - x) * rC;
    }
    followerEnv = env;
    globalMod.follower = juce::jlimit (0.0f, 1.0f, env);
    shownFollower = globalMod.follower;
}

//==============================================================================
// Mod-matrix slots aimed at the effects. Macros and the mod wheel act directly; per-voice sources
// (LFOs, envelopes, velocity...) come from the newest sounding voice.
void HypernovaAudioProcessor::applyGlobalModulation (FxSettings& fx)
{
    const Voice* newest = nullptr;
    for (auto& v : voices)
        if (v.isActive() && (newest == nullptr || v.age > newest->age)) newest = &v;

    float d[NumDest] {};
    bool any = false;
    for (size_t i = 0; i < blockSettings.mod.size(); ++i)
    {
        const auto& slot = blockSettings.mod[i];
        if (! isGlobalDest (slot.dest) || slot.src == SrcNone) continue;
        float v = 0;
        // Sources that don't belong to a voice are shaped here; the rest come from the newest voice,
        // where the slot's shape and slew have already been applied.
        if (slot.src >= SrcMacro1 && slot.src <= SrcMacro4) v = shapeMod (slot.shape, globalMod.macros[(size_t) (slot.src - SrcMacro1)]);
        else if (slot.src == SrcModWheel) v = shapeMod (slot.shape, globalMod.modWheel);
        else if (slot.src == SrcFollow) v = shapeMod (slot.shape, globalMod.follower);
        else if (newest != nullptr) v = newest->slotValue[i];
        d[slot.dest] += v * slot.amount;
        any = true;
    }
    if (! any) return;
    auto clamp01 = [] (float x) { return juce::jlimit (0.0f, 1.0f, x); };
    fx.distMix = clamp01 (fx.distMix + d[DDistFx]);
    fx.distDrive = clamp01 (fx.distDrive + d[DDistFx] * 0.5f);
    fx.ott = clamp01 (fx.ott + d[DOttFx]);
    fx.chorusMix = clamp01 (fx.chorusMix + d[DChorusFx]);
    fx.delayMix = clamp01 (fx.delayMix + d[DDelayFx] * 0.6f);
    fx.reverbMix = clamp01 (fx.reverbMix + d[DReverbFx] * 0.7f);
    fx.shimmer = clamp01 (fx.shimmer + d[DShimmerFx]);

    // Every other effect control: the modulation moves it through its own range (skewed where the knob is).
    auto moved = [&] (int dest, float base)
    {
        auto* p = fxModParam[(size_t) dest];
        if (p == nullptr || std::abs (d[dest]) < 1.0e-5f) return base;
        return p->convertFrom0to1 (clamp01 (p->convertTo0to1 (base) + d[dest]));
    };
    fx.fxFilterFreq = moved (DFxFltFreq, fx.fxFilterFreq);
    fx.fxFilterRes = moved (DFxFltRes, fx.fxFilterRes);
    fx.fxFilterDepth = moved (DFxFltSweep, fx.fxFilterDepth);
    fx.delayFeedback = moved (DDelayFb, fx.delayFeedback);
    fx.delayTone = moved (DDelayTone, fx.delayTone);
    fx.reverbSize = moved (DReverbSize, fx.reverbSize);
    fx.flangerRate = moved (DFlangRate, fx.flangerRate);
    fx.flangerDepth = moved (DFlangDepth, fx.flangerDepth);
    fx.flangerFeedback = moved (DFlangFb, fx.flangerFeedback);
    fx.flangerMix = moved (DFlangMix, fx.flangerMix);
    fx.tapeWobble = moved (DTapeWow, fx.tapeWobble);
    fx.tapeNoise = moved (DTapeNoise, fx.tapeNoise);
    fx.tapeSat = moved (DTapeSat, fx.tapeSat);
    fx.gateDepth = moved (DGateDepth, fx.gateDepth);
    fx.gateShape = moved (DGateShape, fx.gateShape);
    fx.panDepth = moved (DPanDepth, fx.panDepth);
    fx.pitchMix = moved (DShiftMix, fx.pitchMix);
    fx.eqLow = moved (DEqLow, fx.eqLow);
    fx.eqHigh = moved (DEqHigh, fx.eqHigh);
    fx.width = moved (DWidth, fx.width);
    fx.rackMix = moved (DRackMix, fx.rackMix);
    fx.crushMix = moved (DCrushMix, fx.crushMix);
    fx.speakerMix = moved (DSpeakerMix, fx.speakerMix);
    fx.crushBits = juce::jlimit (1.0f, 16.0f, fx.crushBits + d[DCrushBits] * 8.0f);
    fx.chorusRate = moved (DChorusRate, fx.chorusRate);
    fx.distMix = moved (DDistMix, fx.distMix);
    fx.lowLevelDb = moved (DLowLevel, fx.lowLevelDb);
    fx.lowDuck = moved (DLowDuck, fx.lowDuck);
    fx.lowDrive = moved (DLowDrive, fx.lowDrive);
    fx.eqMidGain = moved (DEqMid, fx.eqMidGain);
    fx.eqMidFreq = moved (DEqMidFreq, fx.eqMidFreq);
}

// Arpeggiator: swallows incoming notes and plays them back as a pattern, locked to the host when it's playing.
void HypernovaAudioProcessor::runArpeggiator (juce::MidiBuffer& midi, int numSamples, double ppq, bool playing)
{
    const bool on = param ("arpOn") > 0.5f;
    if (! on)
    {
        if (arpWasOn)
        {
            if (arpNote >= 0) midi.addEvent (juce::MidiMessage::noteOff (1, arpNote), 0);
            arpNote = -1;
            arpKeys.clear();
            arpWasOn = false;
        }
        return;
    }
    arpWasOn = true;

    // Collect key changes, pass everything else through.
    arpOut.clear();
    bool firstPress = false;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            if (arpKeys.empty()) firstPress = true;
            arpKeys.erase (std::remove (arpKeys.begin(), arpKeys.end(), m.getNoteNumber()), arpKeys.end());
            arpKeys.push_back (m.getNoteNumber());
            arpVelocity[(size_t) m.getNoteNumber()] = m.getFloatVelocity();
        }
        else if (m.isNoteOff())
            arpKeys.erase (std::remove (arpKeys.begin(), arpKeys.end(), m.getNoteNumber()), arpKeys.end());
        else
            arpOut.addEvent (m, meta.samplePosition);
    }

    const double stepBeats = arpRateBeats ((int) param ("arpRate"));
    const double gate = param ("arpGate");
    const int octaves = (int) param ("arpOct");
    const int mode = (int) param ("arpMode");
    const double beatsPerSample = bpm / 60.0 / sampleRateNow;

    if (arpKeys.empty())
    {
        if (arpNote >= 0) arpOut.addEvent (juce::MidiMessage::noteOff (1, arpNote), 0);
        arpNote = -1;
        arpStep = 0;
        arpOffBeat = -1;
    }

    double beat = playing && ppq >= 0.0 ? ppq : arpBeat;
    if (firstPress && ! (playing && ppq >= 0.0))
        beat = arpBeat = std::ceil (arpBeat / stepBeats) * stepBeats - beatsPerSample * 0.5; // free-running: fire straight away

    auto sequenceNote = [&] (int step) -> int
    {
        std::vector<int> keys (arpKeys);
        if (mode != 4) std::sort (keys.begin(), keys.end());
        std::vector<int> seq;
        for (int o = 0; o < octaves; ++o)
            for (int k : keys) seq.push_back (k + 12 * o);
        if (mode == 1) std::reverse (seq.begin(), seq.end());
        if (mode == 2 && seq.size() > 2)
            for (int i = (int) seq.size() - 2; i >= 1; --i) seq.push_back (seq[(size_t) i]);
        if (mode == 3) return seq[(size_t) arpRandom.nextInt ((int) seq.size())];
        return seq[(size_t) (step % (int) seq.size())];
    };

    const double blockEnd = beat + beatsPerSample * numSamples;
    // Pending note-off from the previous block.
    if (arpNote >= 0 && arpOffBeat >= 0 && arpOffBeat < blockEnd)
    {
        arpOut.addEvent (juce::MidiMessage::noteOff (1, arpNote), juce::jlimit (0, numSamples - 1, (int) ((arpOffBeat - beat) / beatsPerSample)));
        arpNote = -1;
        arpOffBeat = -1;
    }
    if (! arpKeys.empty())
    {
        for (double b = std::ceil (beat / stepBeats - 1.0e-9) * stepBeats; b < blockEnd; b += stepBeats)
        {
            const int at = juce::jlimit (0, numSamples - 1, (int) ((b - beat) / beatsPerSample));
            if (arpNote >= 0) arpOut.addEvent (juce::MidiMessage::noteOff (1, arpNote), at);
            const int n = juce::jlimit (0, 127, sequenceNote (arpStep++));
            arpOut.addEvent (juce::MidiMessage::noteOn (1, n, juce::jmax (0.05f, arpVelocity[(size_t) arpKeys.back()])), at);
            arpNote = n;
            arpOffBeat = b + stepBeats * gate;
            if (arpOffBeat < blockEnd)
            {
                arpOut.addEvent (juce::MidiMessage::noteOff (1, arpNote), juce::jlimit (0, numSamples - 1, (int) ((arpOffBeat - beat) / beatsPerSample)));
                arpNote = -1;
                arpOffBeat = -1;
            }
        }
    }
    arpBeat = blockEnd;
    midi.swapWith (arpOut);
}

//==============================================================================
const juce::String HypernovaAudioProcessor::getProgramName (int index)
{
    const auto& p = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? juce::String (p[(size_t) index].name) : juce::String();
}

void HypernovaAudioProcessor::applyPresetValues (const std::vector<std::pair<const char*, float>>& values)
{
    apvts.state.setProperty ("fxOrder", fxOrderText (defaultFxOrder()), &undoManager); // every sound starts from the standard rack
    apvts.state.setProperty ("fxRack", "", &undoManager);                               // showing just the effects it uses
    for (int l = 0; l < NumLfo; ++l) apvts.state.removeProperty ("lfo" + juce::String (l + 1) + "Curve", &undoManager); // drawn LFOs start fresh
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    for (const auto& [id, v] : values)
        setParam (id, v);
}

void HypernovaAudioProcessor::loadFactoryPreset (int index)
{
    undoManager.beginNewTransaction ("Load preset");
    const auto& presets = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    const auto& p = presets[(size_t) index];
    applyPresetValues (p.values);
    if (index > 0) setParam ("volume", presetTrim (p.name));
    {
        const juce::ScopedLock sl (nameLock);
        presetName = p.name;
        presetCategory = p.category;
        for (int i = 0; i < 4; ++i) macroNames[(size_t) i] = p.macros[(size_t) i];
    }
    currentProgram = index;
    panic();
    ++presetVersion;
}

//==============================================================================
void HypernovaAudioProcessor::syncFxOrder()
{
    const auto order = parseFxOrder (apvts.state.getProperty ("fxOrder").toString());
    juce::uint64 packed = 0;
    for (int i = 0; i < NumFx; ++i) packed |= (juce::uint64) (order[(size_t) i] & 0xf) << (4 * i);
    fxOrderPacked.store (packed);
    ++parameterChanges;
}

FxOrder HypernovaAudioProcessor::getFxOrder() const { return parseFxOrder (apvts.state.getProperty ("fxOrder").toString()); }

//==============================================================================
// Drawn LFO shapes.
juce::String HypernovaAudioProcessor::defaultLfoCurve() { return "0:-1 0.3:1 0.55:0.1 0.8:0.5"; }

std::vector<juce::Point<float>> HypernovaAudioProcessor::parseLfoCurve (const juce::String& text)
{
    std::vector<juce::Point<float>> pts;
    for (auto& t : juce::StringArray::fromTokens (text, " ", ""))
        if (t.contains (":"))
            pts.push_back ({ juce::jlimit (0.0f, 1.0f, t.upToFirstOccurrenceOf (":", false, false).getFloatValue()),
                             juce::jlimit (-1.0f, 1.0f, t.fromFirstOccurrenceOf (":", false, false).getFloatValue()) });
    std::sort (pts.begin(), pts.end(), [] (auto a, auto b) { return a.x < b.x; });
    if (pts.size() < 2) return parseLfoCurve (defaultLfoCurve());
    return pts;
}

juce::String HypernovaAudioProcessor::lfoCurveText (const std::vector<juce::Point<float>>& pts)
{
    juce::StringArray out;
    for (auto p : pts) out.add (juce::String (p.x, 4) + ":" + juce::String (p.y, 4));
    return out.joinIntoString (" ");
}

juce::String HypernovaAudioProcessor::lfoCurve (int lfo) const
{
    const auto v = apvts.state.getProperty ("lfo" + juce::String (lfo + 1) + "Curve").toString();
    return v.isNotEmpty() ? v : defaultLfoCurve();
}

void HypernovaAudioProcessor::setLfoCurve (int lfo, const juce::String& points)
{
    apvts.state.setProperty ("lfo" + juce::String (lfo + 1) + "Curve", lfoCurveText (parseLfoCurve (points)), &undoManager);
}

// Rebuilds the tables for the audio thread: straight lines between the points, wrapping from the last
// point round to the first, so the cycle joins up.
void HypernovaAudioProcessor::syncLfoTables()
{
    for (int l = 0; l < NumLfo; ++l)
    {
        const auto pts = parseLfoCurve (lfoCurve (l));
        const int side = 1 - lfoTableSide[(size_t) l].load();
        auto& t = lfoTables[(size_t) side][(size_t) l];
        for (int i = 0; i <= LfoTableSize; ++i)
        {
            const float x = (float) i / (float) LfoTableSize;
            size_t k = 0;
            while (k < pts.size() && pts[k].x <= x) ++k;
            const auto a = k == 0 ? juce::Point<float> (pts.back().x - 1.0f, pts.back().y) : pts[k - 1];
            const auto b = k == pts.size() ? juce::Point<float> (pts.front().x + 1.0f, pts.front().y) : pts[k];
            const float span = b.x - a.x;
            t[(size_t) i] = span > 1.0e-6f ? a.y + (b.y - a.y) * (x - a.x) / span : b.y;
        }
        lfoTableSide[(size_t) l].store (side, std::memory_order_release);
        lfoDrawnEnd[(size_t) l].store (pts.back().x);
    }
    ++parameterChanges;
}

void HypernovaAudioProcessor::setFxOrder (const FxOrder& order)
{
    if (order == getFxOrder()) return;
    undoManager.beginNewTransaction ("Reorder effects");
    apvts.state.setProperty ("fxOrder", fxOrderText (order), &undoManager);
}

const char* HypernovaAudioProcessor::fxOnParam (int id)
{
    static const char* ids[] = { "distOn", "tapeOn", "ottOn", "shiftOn", "chorusOn", "flangOn", "fxFltOn", "gateOn", "dlyOn", "verbOn", "eqOn", "crushOn", "spkOn" };
    return ids[juce::jlimit (0, NumFx - 1, id)];
}

bool HypernovaAudioProcessor::fxAudible (int id) const
{
    auto v = [this] (const char* p) { return apvts.getRawParameterValue (p)->load(); };
    if (v (fxOnParam (id)) < 0.5f) return false;
    switch (id)
    {
        case FxDist:    return v ("distMix") > 0.001f;
        case FxTape:    return v ("tapeWow") > 0.001f || v ("tapeNoise") > 0.001f || v ("tapeSat") > 0.001f;
        case FxOtt:     return v ("ott") > 0.001f;
        case FxPitch:   return v ("shiftMix") > 0.001f;
        case FxChorus:  return v ("chorusMix") > 0.001f;
        case FxFlanger: return v ("flangMix") > 0.001f;
        case FxFilter:  return v ("fxFltFreq") < 19000.0f || v ("fxFltDepth") > 0.001f || (int) v ("fxFltType") != 0;
        case FxGate:    return v ("gateDepth") > 0.001f || v ("panDepth") > 0.001f;
        case FxDelay:   return v ("dlyMix") > 0.001f;
        case FxReverb:  return v ("verbMix") > 0.001f;
        case FxEq:      return std::abs (v ("eqLow")) > 0.05f || std::abs (v ("eqHigh")) > 0.05f || std::abs (v ("eqMidGain")) > 0.05f
                               || v ("eqLowCut") > 21.0f || v ("eqHighCut") < 19900.0f;
        case FxCrush:   return v ("crushMix") > 0.001f;
        case FxSpeaker: return v ("spkMix") > 0.001f;
        default:        return false;
    }
}

std::vector<int> HypernovaAudioProcessor::rackEffects() const
{
    const auto added = juce::StringArray::fromTokens (apvts.state.getProperty ("fxRack").toString(), ",", {});
    std::vector<int> out;
    for (auto id : getFxOrder())
        if (fxAudible (id) || (added.contains (juce::String ((int) id)) && apvts.getRawParameterValue (fxOnParam (id))->load() > 0.5f))
            out.push_back (id);
    return out;
}

void HypernovaAudioProcessor::addToRack (int id)
{
    undoManager.beginNewTransaction ("Add effect");
    auto added = juce::StringArray::fromTokens (apvts.state.getProperty ("fxRack").toString(), ",", {});
    added.removeEmptyStrings();
    added.addIfNotAlreadyThere (juce::String (id));
    apvts.state.setProperty ("fxRack", added.joinIntoString (","), &undoManager);
    setParam (fxOnParam (id), 1.0f);
    if (fxAudible (id)) return;
    // Added effects start audible, at an amount that's clearly there without taking over.
    switch (id)
    {
        case FxDist:    setParam ("distMix", 0.5f); break;
        case FxTape:    setParam ("tapeWow", 0.25f); setParam ("tapeSat", 0.3f); break;
        case FxOtt:     setParam ("ott", 0.35f); break;
        case FxPitch:   setParam ("shiftMix", 0.35f); if (std::abs (apvts.getRawParameterValue ("shiftSemis")->load()) < 0.5f) setParam ("shiftSemis", 12.0f); break;
        case FxChorus:  setParam ("chorusMix", 0.4f); break;
        case FxFlanger: setParam ("flangMix", 0.4f); break;
        case FxFilter:  setParam ("fxFltFreq", 2500.0f); break;
        case FxGate:    setParam ("gateDepth", 0.7f); break;
        case FxDelay:   setParam ("dlyMix", 0.3f); break;
        case FxReverb:  setParam ("verbMix", 0.3f); break;
        case FxEq:      setParam ("eqHigh", 3.0f); break;
        case FxCrush:   setParam ("crushMix", 0.5f); setParam ("crushBits", 8.0f); setParam ("crushRate", 8000.0f); break;
        case FxSpeaker: setParam ("spkMix", 0.7f); break;
        default: break;
    }
}

void HypernovaAudioProcessor::removeFromRack (int id)
{
    undoManager.beginNewTransaction ("Remove effect");
    auto added = juce::StringArray::fromTokens (apvts.state.getProperty ("fxRack").toString(), ",", {});
    added.removeString (juce::String (id));
    apvts.state.setProperty ("fxRack", added.joinIntoString (","), &undoManager);
    setParam (fxOnParam (id), 0.0f);
}

juce::String HypernovaAudioProcessor::fxParamPrefix (int fxId)
{
    const juce::String on (fxOnParam (fxId));
    return on.dropLastCharacters (2); // "distOn" -> "dist"
}

void HypernovaAudioProcessor::resetFx (int fxId)
{
    const auto prefix = fxParamPrefix (fxId);
    undoManager.beginNewTransaction ("Reset " + fxRackNames()[fxId]);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->getParameterID().startsWith (prefix) && rp->getParameterID() != juce::String (fxOnParam (fxId))
                && ! rp->getParameterID().endsWith ("Bus"))   // which bus it sits on is routing, not a control
                rp->setValueNotifyingHost (rp->getDefaultValue());
}

void HypernovaAudioProcessor::moveFxBy (int fxId, int places)
{
    auto order = getFxOrder();
    const auto here = std::find (order.begin(), order.end(), (juce::uint8) fxId);
    if (here == order.end()) return;
    // Only the effects in the rack count as steps, so moving skips the ones that aren't shown.
    const auto rack = rackEffects();
    const auto at = std::find (rack.begin(), rack.end(), fxId);
    if (at == rack.end()) return;
    const int to = juce::jlimit (0, (int) rack.size() - 1, (int) (at - rack.begin()) + places);
    if (to == (int) (at - rack.begin())) return;
    auto shown = rack;
    shown.erase (shown.begin() + (at - rack.begin()));
    shown.insert (shown.begin() + to, fxId);
    // Write the new order back into the full chain, keeping effects that aren't in the rack where they are.
    std::vector<int> slots;
    for (int i = 0; i < NumFx; ++i)
        if (std::find (rack.begin(), rack.end(), (int) order[(size_t) i]) != rack.end()) slots.push_back (i);
    for (size_t k = 0; k < slots.size() && k < shown.size(); ++k) order[(size_t) slots[k]] = (juce::uint8) shown[k];
    undoManager.beginNewTransaction ("Move " + fxRackNames()[fxId]);
    setFxOrder (order);
}

bool HypernovaAudioProcessor::isFxParam (const juce::String& id)
{
    static const char* prefixes[] = { "dist", "ott", "chorus", "dly", "verb", "eq", "flang", "tape", "gate", "pan", "fxFlt", "shift" };
    for (auto* p : prefixes) if (id.startsWith (p)) return true;
    return id == "width" || id == "monoBass";
}

juce::File HypernovaAudioProcessor::chainFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Application Support/Arrow/Hypernova/Effect Chains");
}

bool HypernovaAudioProcessor::saveChain (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    juce::XmlElement xml ("HypernovaChain");
    xml.setAttribute ("name", clean);
    xml.setAttribute ("order", fxOrderText (getFxOrder()));
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (isFxParam (rp->getParameterID()))
                xml.createNewChildElement ("P")->setAttribute (rp->getParameterID(), (double) rp->convertFrom0to1 (rp->getValue()));
    chainFolder().createDirectory();
    return xml.writeTo (chainFolder().getChildFile (clean + ".hnchain"));
}

bool HypernovaAudioProcessor::loadChain (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("HypernovaChain")) return false;
    undoManager.beginNewTransaction ("Load effect chain");
    for (auto* e : xml->getChildIterator())
        for (int i = 0; i < e->getNumAttributes(); ++i)
            if (isFxParam (e->getAttributeName (i)) && apvts.getParameter (e->getAttributeName (i)) != nullptr)
                setParam (e->getAttributeName (i), (float) e->getAttributeValue (i).getDoubleValue());
    apvts.state.setProperty ("fxOrder", fxOrderText (parseFxOrder (xml->getStringAttribute ("order"))), &undoManager);
    return true;
}

juce::File HypernovaAudioProcessor::userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Application Support/Arrow/Hypernova/Presets");
}

// Sound packs installed for every user by the installer (read-only), e.g. the FOUNDERS PACK.
juce::File HypernovaAudioProcessor::wavetableFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Application Support/Arrow/Hypernova/Wavetables");
}

juce::StringArray HypernovaAudioProcessor::installedWavetables()
{
    juce::StringArray names;
    for (const auto& f : wavetableFolder().findChildFiles (juce::File::findFiles, false, "*.hnwt"))
        names.add (f.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}

// Reads an audio file (or a previously imported .hnwt) and turns it into a 32-frame table. WAVs whose length
// is a multiple of 2048 are treated as standard wavetable files; anything else is sliced evenly across its length.
juce::String HypernovaAudioProcessor::importWavetable (const juce::File& file, int osc, juce::String& error)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::vector<float> mono;
    int cycle = 0;
    juce::String name = file.getFileNameWithoutExtension();

    if (file.hasFileExtension ("hnwt"))
    {
        juce::FileInputStream in (file);
        if (! in.openedOk()) { error = "Couldn't read " + file.getFileName(); return {}; }
        mono.resize ((size_t) ab::wtFrames * ab::wtBaseSize);
        if (in.read (mono.data(), (int) (mono.size() * sizeof (float))) != (int) (mono.size() * sizeof (float)))
        { error = file.getFileName() + " is not a Hypernova wavetable."; return {}; }
        cycle = ab::wtBaseSize;
    }
    else
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr) { error = "Couldn't read " + file.getFileName() + " as audio."; return {}; }
        const int len = (int) juce::jmin ((juce::int64) (8 * 1024 * 1024), reader->lengthInSamples);
        if (len < 64) { error = file.getFileName() + " is too short to be a wavetable."; return {}; }
        juce::AudioBuffer<float> buf ((int) reader->numChannels, len);
        reader->read (&buf, 0, len, 0, true, true);
        mono.resize ((size_t) len);
        for (int i = 0; i < len; ++i)
        {
            float sum = 0;
            for (int c = 0; c < buf.getNumChannels(); ++c) sum += buf.getSample (c, i);
            mono[(size_t) i] = sum / (float) juce::jmax (1, buf.getNumChannels());
        }
        // A serum-style wavetable file is a whole number of 2048-sample cycles.
        if (len % ab::wtBaseSize == 0 && len >= ab::wtBaseSize * 2) cycle = ab::wtBaseSize;
    }

    auto table = std::make_shared<ab::Wavetable> (ab::WavetableBank::fromAudio (name, mono.data(), (int) mono.size(), cycle));

    // Keep a copy in the user's wavetable folder as 32 raw frames, so presets can refer to it by name.
    wavetableFolder().createDirectory();
    const auto dest = wavetableFolder().getChildFile (name + ".hnwt");
    if (! file.hasFileExtension ("hnwt"))
    {
        juce::TemporaryFile temp (dest);
        {
            juce::FileOutputStream out (temp.getFile());
            if (out.openedOk())
                for (int f = 0; f < ab::wtFrames; ++f)
                    out.write (table->frame (f, 0), (size_t) ab::wtBaseSize * sizeof (float));
        }
        temp.overwriteTargetFileWithTemporary();
    }

    tableCache[name.toStdString()] = table;
    if (juce::isPositiveAndBelow (osc, NumOsc))
    {
        userTableSlot[(size_t) osc] = name;
        userTable[(size_t) osc].store (table.get());
        ++tableVersion;
    }
    return name;
}

// Points an oscillator at an imported table by name, loading it from the wavetable folder if needed.
bool HypernovaAudioProcessor::setUserTable (int osc, const juce::String& name)
{
    if (! juce::isPositiveAndBelow (osc, NumOsc)) return false;
    if (name.isEmpty())
    {
        userTableSlot[(size_t) osc] = {};
        userTable[(size_t) osc].store (nullptr);
        ++tableVersion;
        return true;
    }
    auto it = tableCache.find (name.toStdString());
    if (it == tableCache.end())
    {
        const auto file = wavetableFolder().getChildFile (name + ".hnwt");
        if (! file.existsAsFile()) return false;
        juce::String error;
        return importWavetable (file, osc, error).isNotEmpty();
    }
    userTableSlot[(size_t) osc] = name;
    userTable[(size_t) osc].store (it->second.get());
    ++tableVersion;
    return true;
}

juce::File HypernovaAudioProcessor::packFolder()
{
    return juce::File ("/Library/Application Support/Arrow/Hypernova/Packs");
}

bool HypernovaAudioProcessor::isPresetFile (const juce::File& f)
{
    return f.existsAsFile() && (f.hasFileExtension (presetExtension) || f.hasFileExtension (".abpreset"));
}

juce::Array<juce::File> HypernovaAudioProcessor::presetFilesIn (const juce::File& folder)
{
    auto files = folder.findChildFiles (juce::File::findFiles, true, juce::String ("*") + presetExtension + ";*.abpreset");
    files.sort();
    return files;
}

juce::Array<juce::File> HypernovaAudioProcessor::userPresetFiles()
{
    return presetFilesIn (userPresetFolder());
}

// Writes the current sound as a shareable preset file.
bool HypernovaAudioProcessor::writePresetFile (const juce::File& file, const juce::String& name, const juce::String& author)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", name, nullptr);
    state.setProperty ("author", author, nullptr);
    state.setProperty ("format", "Hypernova preset 1", nullptr);
    for (int i = 0; i < 4; ++i) state.setProperty ("macro" + juce::String (i + 1) + "Name", getMacroName (i), nullptr);
    state.removeProperty ("program", nullptr);
    state.removeProperty ("presetCategory", nullptr);
    // A sound that uses the sampler carries its sample, so a shared preset never has a missing file.
    if (param ("smpOn") > 0.5f) addSampleTo (state);
    auto xml = state.createXml();
    file.getParentDirectory().createDirectory();
    return xml != nullptr && xml->writeTo (file);
}

bool HypernovaAudioProcessor::saveUserPreset (const juce::String& name, const juce::String& author)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    if (! writePresetFile (userPresetFolder().getChildFile (clean + presetExtension), clean, author)) return false;
    {
        const juce::ScopedLock sl (nameLock);
        presetName = clean;
        presetCategory = author.isNotEmpty() ? "User  /  by " + author : juce::String ("User");
    }
    ++presetVersion;
    return true;
}

bool HypernovaAudioProcessor::exportPreset (const juce::File& file, const juce::String& author)
{
    const auto target = file.withFileExtension (presetExtension);
    return writePresetFile (target, target.getFileNameWithoutExtension(), author);
}

bool HypernovaAudioProcessor::loadUserPreset (const juce::File& file)
{
    if (! isPresetFile (file)) return false;
    undoManager.beginNewTransaction ("Load preset");
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr) return false;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType())) return false;
    applyPresetValues ({}); // anything the file doesn't mention (older versions) starts from Init
    takeSampleFrom (state, false);
    state.setProperty ("abSlot", compareSlot(), nullptr); // a preset loads into the A/B slot you're on
    apvts.replaceState (state);
    readCornersFromState();   // Orbit's corners come with the preset
    readSlicesFromState();
    {
        const juce::ScopedLock sl (nameLock);
        presetName = state.getProperty ("presetName", file.getFileNameWithoutExtension()).toString();
        const auto author = state.getProperty ("author").toString();
        const bool inPack = file.isAChildOf (packFolder());
        juce::String where = inPack ? file.getParentDirectory().getFileName() : juce::String ("User");
        if (file.getParentDirectory() != userPresetFolder() && ! inPack && file.isAChildOf (userPresetFolder()))
            where = file.getParentDirectory().getFileName();
        presetCategory = author.isNotEmpty() ? where + "  /  by " + author : where;
        for (int i = 0; i < 4; ++i)
            macroNames[(size_t) i] = state.getProperty ("macro" + juce::String (i + 1) + "Name", "MACRO " + juce::String (i + 1)).toString();
    }
    panic();
    ++presetVersion;
    return true;
}

// Copies preset files (or whole folders of them, e.g. a friend's pack) into the user folder.
// Folders keep their name as a sub-folder so packs stay together. Returns how many sounds were added.
int HypernovaAudioProcessor::importPresets (const juce::Array<juce::File>& items, juce::File* firstImported)
{
    int count = 0;
    auto copyOne = [&] (const juce::File& f, const juce::File& destFolder)
    {
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr || ! xml->hasTagName (apvts.state.getType().toString())) return;
        destFolder.createDirectory();
        auto dest = destFolder.getChildFile (f.getFileNameWithoutExtension() + presetExtension);
        if (dest != f && ! f.copyFileTo (dest)) return;
        if (firstImported != nullptr && count == 0) *firstImported = dest;
        ++count;
    };
    for (const auto& item : items)
    {
        if (item.isDirectory())
        {
            const auto dest = userPresetFolder().getChildFile (juce::File::createLegalFileName (item.getFileName()));
            for (const auto& f : presetFilesIn (item))
                copyOne (f, dest);
        }
        else if (isPresetFile (item))
            copyOne (item, userPresetFolder().getChildFile ("Imported"));
        else if (item.hasFileExtension ("zip"))
        {
            // A sound pack straight from the website: unzip to a temp folder, import it as one pack
            // named after the zip, and bring any wavetables inside along too.
            const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("HypernovaPack-" + juce::Uuid().toString());
            juce::ZipFile zip (item);
            if (zip.getNumEntries() > 0 && zip.uncompressTo (temp, true).wasOk())
            {
                const auto dest = userPresetFolder().getChildFile (juce::File::createLegalFileName (item.getFileNameWithoutExtension()));
                for (const auto& f : temp.findChildFiles (juce::File::findFiles, true, juce::String ("*") + presetExtension + ";*.abpreset"))
                    copyOne (f, dest);
                for (const auto& wt : temp.findChildFiles (juce::File::findFiles, true, "*.hnwt"))
                {
                    wavetableFolder().createDirectory();
                    wt.copyFileTo (wavetableFolder().getChildFile (wt.getFileName()));
                }
            }
            temp.deleteRecursively();
        }
    }
    return count;
}

void HypernovaAudioProcessor::stepPreset (int delta)
{
    const auto& presets = factoryPresets();
    const int n = (int) presets.size();
    int next = currentProgram;
    for (int tries = 0; tries < n; ++tries)
    {
        next = (next + delta + n) % n;
        if (! isRetiredPreset (presets[(size_t) next].name)) break; // retired sounds are skipped
    }
    loadFactoryPreset (next);
}

void HypernovaAudioProcessor::setMacroName (int i, const juce::String& n)
{
    {
        const juce::ScopedLock sl (nameLock);
        macroNames[(size_t) i] = n.toUpperCase().substring (0, 10);
    }
    ++presetVersion;
}

// Mutate: move every continuous control a random amount (up to `amount` of its range) from where it is.
// Levels, volume and structural choices stay put so the sound keeps its identity.
juce::String HypernovaAudioProcessor::sectionName (int s)
{
    static const char* names[] = { "oscillators", "filter", "envelopes and LFOs", "effects" };
    return juce::isPositiveAndBelow (s, NumSections) ? names[s] : juce::String();
}

// Which section a parameter belongs to, used by the section dice and the padlocks.
bool HypernovaAudioProcessor::paramInSection (const juce::String& id, int section)
{
    const bool osc = id.startsWith ("a") || id.startsWith ("b") || id.startsWith ("sub") || id.startsWith ("noise")
                     || id.startsWith ("x") || id == "drift" || id.startsWith ("drop");
    const bool filter = id.startsWith ("flt") || id == "cutoff" || id == "res";
    const bool modSec = id.startsWith ("amp") || id.startsWith ("mod") || id.startsWith ("lfo");
    const bool fx = id.startsWith ("dist") || id == "ott" || id.startsWith ("chorus") || id.startsWith ("dly")
                    || id.startsWith ("verb") || id.startsWith ("eq") || id.startsWith ("flang") || id.startsWith ("tape")
                    || id.startsWith ("gate") || id.startsWith ("pan") || id.startsWith ("fxFlt") || id.startsWith ("shift")
                    || id == "width";
    switch (section)
    {
        case SecOsc:    return osc && ! filter;
        case SecFilter: return filter;
        case SecEnvLfo: return modSec;
        case SecFx:     return fx;
        default:        return false;
    }
}

// Re-rolls one part of the sound and leaves the rest alone. Locked sections are never touched.
void HypernovaAudioProcessor::mutateSection (int section, float amount)
{
    if (! juce::isPositiveAndBelow (section, NumSections) || sectionLocked[(size_t) section].load()) return;
    undoManager.beginNewTransaction ("Re-roll " + sectionName (section));
    juce::Random r;
    static const juce::StringArray keep { "volume", "macro1", "macro2", "macro3", "macro4" };
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr || keep.contains (rp->getParameterID())) continue;
        if (! paramInSection (rp->getParameterID(), section)) continue;
        if (dynamic_cast<juce::AudioParameterFloat*> (rp) == nullptr) continue;
        const float v = rp->getValue();
        rp->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v + (r.nextFloat() * 2.0f - 1.0f) * amount));
    }
    {
        const juce::ScopedLock sl (nameLock);
        if (! presetName.endsWith ("*")) presetName << " *";
    }
    ++presetVersion;
}

void HypernovaAudioProcessor::mutate (float amount)
{
    undoManager.beginNewTransaction ("Mutate");
    juce::Random r;
    static const juce::StringArray keep { "volume", "aLevel", "bLevel", "subLevel", "macro1", "macro2", "macro3", "macro4", "mode",
                                          "bendRange", "transpose", "tune", "arpOn", "chord", "velSens", "aOct", "bOct", "subOct" };
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr || keep.contains (rp->getParameterID())) continue;
        if (dynamic_cast<juce::AudioParameterFloat*> (rp) == nullptr) continue; // choices/toggles/ints stay
        bool locked = false;
        for (int sec = 0; sec < NumSections; ++sec)
            locked = locked || (sectionLocked[(size_t) sec].load() && paramInSection (rp->getParameterID(), sec));
        if (locked) continue;
        const float v = rp->getValue();
        rp->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v + (r.nextFloat() * 2.0f - 1.0f) * amount));
    }
    {
        const juce::ScopedLock sl (nameLock);
        if (! presetName.endsWith ("*")) presetName << " *";
    }
    ++presetVersion;
}

// "Happy accident": a random but musical bass patch.
void HypernovaAudioProcessor::randomize()
{
    undoManager.beginNewTransaction ("Randomize");
    juce::Random r;
    auto pick = [&] (std::initializer_list<int> v) { return *(v.begin() + r.nextInt ((int) v.size())); };
    auto range = [&] (float lo, float hi) { return lo + r.nextFloat() * (hi - lo); };

    applyPresetValues ({});
    setParam ("aTable", (float) r.nextInt (NumTables));
    setParam ("aPos", range (0.0f, 1.0f));
    if (r.nextFloat() < 0.35f)
    {
        setParam ("aWarp", (float) (1 + r.nextInt (NumWarps - 1)));
        setParam ("aWarpAmt", range (0.1f, 0.6f));
    }
    setParam ("aUni", (float) pick ({ 1, 1, 2, 3, 5 }));
    setParam ("aDetune", range (0.05f, 0.35f));
    if (r.nextBool())
    {
        setParam ("bOn", 1);
        setParam ("bTable", (float) r.nextInt (NumTables));
        setParam ("bPos", range (0.0f, 1.0f));
        setParam ("bOct", (float) pick ({ -1, 0, 0, 1 }));
        setParam ("bLevel", range (0.2f, 0.6f));
    }
    if (r.nextFloat() < 0.6f)
    {
        setParam ("subOn", 1);
        setParam ("subOct", -1);
        setParam ("subLevel", range (0.3f, 0.6f));
    }
    setParam ("mode", (float) pick ({ 1, 2 }));
    setParam ("glide", range (0.0f, 0.12f));
    setParam ("fltType", (float) pick ({ 0, 1, 1, 5, 3 }));
    setParam ("cutoff", std::exp (range (std::log (200.0f), std::log (6000.0f))));
    setParam ("res", range (0.0f, 0.6f));
    setParam ("fltEnv", range (0.0f, 0.6f));
    setParam ("modD", range (0.05f, 0.6f));
    if (r.nextFloat() < 0.4f)
    {
        setParam ("dropAmt", (float) pick ({ 5, 7, 12, 12, 19, 24 }));
        setParam ("dropTime", range (0.015f, 0.08f));
    }
    if (r.nextFloat() < 0.6f)
    {
        setParam ("lfo1Shape", (float) r.nextInt (LDrawn)); // the built-in shapes (a drawn one needs drawing)
        setParam ("lfo1Sync", (float) pick ({ 5, 6, 7, 8, 11 }));
        setParam ("mod1Src", 1);
        setParam ("mod1Dest", (float) pick ({ DAPos, DCutoff, DAWarp }));
        setParam ("mod1Amt", range (0.2f, 0.5f));
    }
    setParam ("mod2Src", SrcMacro1);
    setParam ("mod2Dest", DAPos);
    setParam ("mod2Amt", 0.5f);
    setParam ("mod3Src", SrcMacro2);
    setParam ("mod3Dest", DCutoff);
    setParam ("mod3Amt", 0.6f);
    setParam ("distType", (float) r.nextInt (NumDistTypes));
    setParam ("distDrive", range (0.1f, 0.5f));
    setParam ("distMix", range (0.2f, 0.8f));
    setParam ("ott", range (0.0f, 0.5f));
    setParam ("volume", -9.0f);
    {
        const juce::ScopedLock sl (nameLock);
        presetName = "Happy Accident";
        presetCategory = "Random";
        macroNames = { "MORPH", "OPEN", "MACRO 3", "MACRO 4" };
    }
    panic();
    ++presetVersion;
}

//==============================================================================
void HypernovaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    {
        const juce::ScopedLock sl (nameLock);
        state.setProperty ("presetName", presetName, nullptr);
        state.setProperty ("presetCategory", presetCategory, nullptr);
        for (int i = 0; i < 4; ++i) state.setProperty ("macro" + juce::String (i + 1) + "Name", macroNames[(size_t) i], nullptr);
    }
    state.setProperty ("program", currentProgram, nullptr);
    for (int o = 0; o < NumOsc; ++o)
        if (o < 2 || userTableSlot[(size_t) o].isNotEmpty())
            state.setProperty (oscPrefix (o) + "UserTable", userTableSlot[(size_t) o], nullptr);
    state.setProperty ("uiAnimation", uiAnimation.load(), nullptr);
    state.setProperty ("uiScale", uiScalePercent.load(), nullptr);
    addSampleTo (state);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void HypernovaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.hasType (apvts.state.getType())) return;
        takeSampleFrom (state, true);
        apvts.replaceState (state);
        readCornersFromState();   // Orbit's captured sounds travel with the session
        readSlicesFromState();
        const juce::ScopedLock sl (nameLock);
        presetName = state.getProperty ("presetName", "Init").toString();
        presetCategory = state.getProperty ("presetCategory", "").toString();
        for (int i = 0; i < 4; ++i)
            macroNames[(size_t) i] = state.getProperty ("macro" + juce::String (i + 1) + "Name", "MACRO " + juce::String (i + 1)).toString();
        currentProgram = state.getProperty ("program", 0);
        for (int o = 0; o < NumOsc; ++o)
            setUserTable (o, state.getProperty (oscPrefix (o) + "UserTable", "").toString());
        uiAnimation = (int) state.getProperty ("uiAnimation", 0);
        uiScalePercent = (int) state.getProperty ("uiScale", 100);
    }
    ++presetVersion;
}

//==============================================================================
void HypernovaAudioProcessor::compareSwitch (int slot)
{
    const int from = compareSlot();
    slot = juce::jlimit (0, 1, slot);
    if (slot == from) return;
    juce::MemoryBlock now;
    getStateInformation (now);
    compareStore[(size_t) from] = now;
    auto& next = compareStore[(size_t) slot];
    if (next.getSize() == 0) next = now; // the first time, B starts as a copy of A
    auto xml = getXmlFromBinary (next.getData(), (int) next.getSize());
    if (xml == nullptr) return;
    auto target = juce::ValueTree::fromXml (*xml);
    // Applied value by value (not by swapping the whole state) so the switch is one undoable step and
    // the edits made before it stay undoable too.
    undoManager.beginNewTransaction (slot == 0 ? "Compare: A" : "Compare: B");
    takeSampleFrom (target, true);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto child = target.getChildWithProperty ("id", rp->paramID);
            const float v = child.isValid() ? rp->convertTo0to1 ((float) child.getProperty ("value")) : rp->getDefaultValue();
            if (std::abs (rp->getValue() - v) > 1.0e-7f) rp->setValueNotifyingHost (v);
        }
    static const juce::StringArray notSound { "presetName", "presetCategory", "program", "uiAnimation", "uiScale", "abSlot" };
    for (int i = 0; i < target.getNumProperties(); ++i)
    {
        const auto name = target.getPropertyName (i);
        if (notSound.contains (name.toString()) || name.toString().startsWith ("macro") || name.toString().endsWith ("UserTable")) continue;
        apvts.state.setProperty (name, target.getProperty (name), &undoManager);
    }
    {
        const juce::ScopedLock sl (nameLock);
        presetName = target.getProperty ("presetName", "Init").toString();
        presetCategory = target.getProperty ("presetCategory", "").toString();
        for (int i = 0; i < 4; ++i)
            macroNames[(size_t) i] = target.getProperty ("macro" + juce::String (i + 1) + "Name", "MACRO " + juce::String (i + 1)).toString();
    }
    currentProgram = target.getProperty ("program", 0);
    for (int o = 0; o < NumOsc; ++o)
        setUserTable (o, target.getProperty (oscPrefix (o) + "UserTable", "").toString());
    // Orbit's captured sounds are part of the sound, so they switch over with it.
    apvts.state.removeChild (apvts.state.getChildWithName ("orbit"), &undoManager);
    if (auto orbit = target.getChildWithName ("orbit"); orbit.isValid())
        apvts.state.appendChild (orbit.createCopy(), &undoManager);
    readCornersFromState();
    apvts.state.setProperty ("abSlot", slot, &undoManager);
    apvts.copyState(); // lands the parameter changes in this step's undo now, not on the next timer tick
    ++presetVersion;
}

void HypernovaAudioProcessor::compareCopyToOther()
{
    juce::MemoryBlock now;
    getStateInformation (now);
    compareStore[(size_t) (1 - compareSlot())] = now;
}

//==============================================================================
// Sampler: loading, keeping samples alive for the audio thread, and saving them inside sessions and presets.
std::shared_ptr<ab::SampleData> HypernovaAudioProcessor::makeSample (const juce::AudioBuffer<float>& buffer, double rate, const juce::String& name)
{
    auto d = std::make_shared<ab::SampleData>();
    d->name = name;
    d->rate = rate;
    d->length = buffer.getNumSamples();
    d->l.assign ((size_t) d->length + 4, 0.0f);
    d->r.assign ((size_t) d->length + 4, 0.0f);
    const int chans = buffer.getNumChannels();
    for (int i = 0; i < d->length; ++i)
    {
        d->l[(size_t) i + 2] = buffer.getSample (0, i);
        d->r[(size_t) i + 2] = buffer.getSample (juce::jmin (1, chans - 1), i);
    }
    // Where the sound starts, and what note it is, so a dropped sample plays in tune from the first key.
    std::vector<float> mono ((size_t) d->length);
    float peak = 0;
    for (int i = 0; i < d->length; ++i)
    {
        mono[(size_t) i] = 0.5f * (d->l[(size_t) i + 2] + d->r[(size_t) i + 2]);
        peak = juce::jmax (peak, std::abs (mono[(size_t) i]));
    }
    const float floor = peak * 0.0056f; // -45 dB under the peak
    while (d->onset < d->length - 1 && std::abs (mono[(size_t) d->onset]) < floor) ++d->onset;
    d->onset = juce::jmax (0, d->onset - (int) (0.002 * rate)); // keep a hair of the lead-in
    d->rootGuess = ab::dsp::detectPitch (mono, rate, juce::jmin (d->length - 1, d->onset + (int) (0.05 * rate)));
    return d;
}

void HypernovaAudioProcessor::installSample (std::shared_ptr<ab::SampleData> d, juce::MemoryBlock flac)
{
    // The audio thread reads through the atomic; the one it may still be reading is kept for a few seconds.
    const auto now = juce::Time::getMillisecondCounter();
    retiredSamples.erase (std::remove_if (retiredSamples.begin(), retiredSamples.end(),
                                          [now] (const auto& r) { return now - r.first > 3000; }),
                          retiredSamples.end());
    if (sampleHeld != nullptr) retiredSamples.push_back ({ now, sampleHeld });
    sampleHeld = std::move (d);
    sampleFlac = std::move (flac);
    currentSample.store (sampleHeld.get(), std::memory_order_release);
    ++sampleVersion;
}

void HypernovaAudioProcessor::clearSample()
{
    if (sampleHeld == nullptr) return;
    installSample (nullptr, {});
}

juce::MemoryBlock HypernovaAudioProcessor::encodeFlac (const juce::AudioBuffer<float>& buffer, double rate)
{
    juce::MemoryBlock block;
    {
        juce::FlacAudioFormat flac;
        auto* stream = new juce::MemoryOutputStream (block, false);
        std::unique_ptr<juce::AudioFormatWriter> writer (flac.createWriterFor (stream, rate, (unsigned int) buffer.getNumChannels(), 24, {}, 5));
        if (writer == nullptr) { delete stream; return {}; }
        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }
    return block;
}

bool HypernovaAudioProcessor::loadSample (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr) { error = "Couldn't read that file as audio"; return false; }
    const double rate = reader->sampleRate > 0 ? reader->sampleRate : 44100.0;
    const auto maxFrames = (juce::int64) (rate * maxSampleSeconds);
    const int frames = (int) juce::jmin (reader->lengthInSamples, maxFrames);
    if (frames < 32) { error = "That file is too short"; return false; }
    const int chans = juce::jlimit (1, 2, (int) reader->numChannels);
    juce::AudioBuffer<float> buffer (chans, frames);
    reader->read (&buffer, 0, frames, 0, true, chans > 1);
    auto d = makeSample (buffer, rate, file.getFileNameWithoutExtension());
    if (reader->lengthInSamples > maxFrames) error = "Trimmed to the first " + juce::String ((int) maxSampleSeconds) + " seconds";
    const float guess = d->rootGuess;
    const float onset = (float) d->onset / (float) juce::jmax (1, d->length);
    installSample (d, encodeFlac (buffer, rate));
    // A new sample starts from its onset, in tune, and switched on: drop it in and play.
    undoManager.beginNewTransaction ("Load sample");
    setParam ("smpOn", 1.0f);
    setParam ("smpStart", onset);
    setParam ("smpEnd", 1.0f);
    if (guess >= 0.0f) setParam ("smpRoot", (float) juce::jlimit (0, 127, juce::roundToInt (guess)));
    if (guess >= 0.0f) setParam ("smpFine", juce::jlimit (-50.0f, 50.0f, (float) (juce::roundToInt (guess) - guess) * 100.0f));
    return true;
}

// Resample: the sound plays itself into the sampler. A second copy of the plugin renders it offline with
// this sound's own state, so whatever is playing here carries on untouched.
bool HypernovaAudioProcessor::resampleSelf (int note, double seconds, juce::String& error)
{
    const double rate = sampleRateNow > 8000.0 ? sampleRateNow : 48000.0;
    seconds = juce::jlimit (0.25, 20.0, seconds);
    note = juce::jlimit (0, 127, note);

    juce::MemoryBlock state;
    getStateInformation (state);
    auto copy = std::make_unique<HypernovaAudioProcessor>();
    copy->setStateInformation (state.getData(), (int) state.getSize());
    copy->bpm = bpm;                       // synced effects and arps keep the session's tempo
    copy->prepareToPlay (rate, 512);

    const int total = (int) (seconds * rate);
    juce::AudioBuffer<float> out (2, total);
    out.clear();
    const int off = (int) (total * 0.75);  // the key is held for three quarters, then it rings out
    for (int pos = 0; pos < total; pos += 512)
    {
        const int n = juce::jmin (512, total - pos);
        float* chans[2] = { out.getWritePointer (0, pos), out.getWritePointer (1, pos) };
        juce::AudioBuffer<float> chunk (chans, 2, n);
        juce::MidiBuffer midi;
        if (pos == 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        if (off >= pos && off < pos + n) midi.addEvent (juce::MidiMessage::noteOff (1, note), off - pos);
        copy->processBlock (chunk, midi);
    }
    copy->releaseResources();

    // Trim the silence at the end, and refuse to make a sample out of nothing.
    int last = total - 1;
    while (last > 0 && juce::jmax (std::abs (out.getSample (0, last)), std::abs (out.getSample (1, last))) < 0.0005f) --last;
    if (last < (int) (0.02 * rate) || out.getMagnitude (0, total) < 0.001f)
    {
        error = "There was nothing to record: play the sound first, or give it longer";
        return false;
    }
    juce::AudioBuffer<float> trimmed (2, last + 1);
    for (int c = 0; c < 2; ++c) trimmed.copyFrom (c, 0, out, c, 0, last + 1);

    juce::String name;
    {
        const juce::ScopedLock sl (nameLock);
        name = presetName.isEmpty() ? juce::String ("Resample") : presetName + " (resampled)";
    }
    installSample (makeSample (trimmed, rate, name), encodeFlac (trimmed, rate));
    undoManager.beginNewTransaction ("Resample");
    setParam ("smpOn", 1.0f);
    setParam ("smpStart", 0.0f);
    setParam ("smpEnd", 1.0f);
    setParam ("smpTrack", 1.0f);
    setParam ("smpRoot", (float) note);
    setParam ("smpSemi", 0.0f);
    setParam ("smpFine", 0.0f);
    setParam ("smpLoop", 0.0f);
    // The recording already has the sound's shape in it, so the sampler's own envelope opens straight up
    // and stays out of the way.
    setParam ("smpLevel", 1.0f);
    setParam ("smpA", 0.001f);
    setParam ("smpD", 0.1f);
    setParam ("smpS", 1.0f);
    setParam ("smpR", 0.05f);
    setParam ("chopOn", 0.0f);
    setSlices ("");
    apvts.copyState();
    return true;
}

void HypernovaAudioProcessor::addSampleTo (juce::ValueTree& state) const
{
    if (sampleHeld == nullptr || sampleFlac.isEmpty()) return;
    juce::ValueTree s ("Sample");
    s.setProperty ("name", sampleHeld->name, nullptr);
    s.setProperty ("rate", sampleHeld->rate, nullptr);
    s.setProperty ("flac", sampleFlac.toBase64Encoding(), nullptr);
    state.appendChild (s, nullptr);
}

// Pulls the embedded sample (if any) out of a loaded state so it doesn't end up inside the parameter tree.
// A session without one clears the sampler; a preset without one leaves the loaded sample alone.
void HypernovaAudioProcessor::takeSampleFrom (juce::ValueTree& state, bool clearIfMissing)
{
    auto s = state.getChildWithName ("Sample");
    if (! s.isValid()) { if (clearIfMissing) clearSample(); return; }
    state.removeChild (s, nullptr);
    juce::MemoryBlock block;
    if (! block.fromBase64Encoding (s.getProperty ("flac").toString())) return;
    juce::FlacAudioFormat flac;
    std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (new juce::MemoryInputStream (block, false), true));
    if (reader == nullptr) return;
    const int frames = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * maxSampleSeconds));
    juce::AudioBuffer<float> buffer ((int) reader->numChannels, frames);
    reader->read (&buffer, 0, frames, 0, true, reader->numChannels > 1);
    installSample (makeSample (buffer, reader->sampleRate, s.getProperty ("name").toString()), std::move (block));
}

juce::AudioProcessorEditor* HypernovaAudioProcessor::createEditor()
{
    return new HypernovaAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HypernovaAudioProcessor();
}
