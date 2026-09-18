#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"

// Builds the FOUNDERS PACK: exclusive sounds that aren't in the factory bank, written as shareable
// .hnpreset files and levelled like the factory presets. MakeFoundersPack <outDir>
namespace
{
    enum Kind { Bass, Lead, Pad };

    struct PackSound
    {
        const char* name;
        Kind kind;
        std::vector<std::pair<const char*, float>> values;
        std::array<const char*, 4> macros;
    };

    struct Head : juce::AudioPlayHead
    {
        double ppq = 0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo i; i.setBpm (124.0); i.setPpqPosition (ppq); i.setIsPlaying (true); return i;
        }
    };

    // Loudest 100 ms RMS (dBFS) of a short phrase, as in SmokeTest --loudness.
    float measure (HypernovaAudioProcessor& p, Head& head, Kind kind)
    {
        const double rate = 48000.0;
        const int total = (int) (4.0 * rate), base = kind == Bass ? 36 : 60;
        std::vector<float> mono ((size_t) total);
        for (int pos = 0; pos < total; pos += 256)
        {
            juce::AudioBuffer<float> buf (2, 256);
            juce::MidiBuffer midi;
            for (int k = 0; k < 4; ++k)
            {
                if (kind == Pad && k > 1) break;
                const int on = kind == Pad ? 0 : (int) (k * 0.9 * rate), off = kind == Pad ? total - 512 : on + (int) (0.7 * rate);
                const int note = base + (k == 1 ? 7 : k == 2 ? 3 : k == 3 ? 5 : 0);
                if (on >= pos && on < pos + 256) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), on - pos);
                if (off >= pos && off < pos + 256) midi.addEvent (juce::MidiMessage::noteOff (1, note), off - pos);
            }
            head.ppq = pos / rate * 124.0 / 60.0;
            p.processBlock (buf, midi);
            for (int s = 0; s < 256 && pos + s < total; ++s)
                mono[(size_t) (pos + s)] = 0.5f * (buf.getSample (0, s) + buf.getSample (1, s));
        }
        const int win = (int) (0.1 * rate);
        double best = 0;
        for (int w = 0; w + win <= total; w += win / 4)
        {
            double sum = 0;
            for (int s = w; s < w + win; ++s) sum += (double) mono[(size_t) s] * mono[(size_t) s];
            best = std::max (best, std::sqrt (sum / win));
        }
        p.panic();
        juce::AudioBuffer<float> flush (2, 256);
        juce::MidiBuffer none;
        p.processBlock (flush, none);
        return juce::Decibels::gainToDecibels ((float) best, -100.0f);
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    outDir.deleteRecursively();
    outDir.createDirectory();

    const std::vector<PackSound> pack
    {
        { "Supernova 808", Bass, { { "aTable", 3 }, { "aPos", 0.55f }, { "bOn", 1 }, { "bTable", 8 }, { "bPos", 0.3f }, { "bOct", 1 }, { "bLevel", 0.15f },
            { "dropAmt", 14 }, { "dropTime", 0.04f }, { "mode", 2 }, { "glide", 0.1f }, { "bendRange", 12 },
            { "ampD", 3.0f }, { "ampS", 0.6f }, { "ampR", 0.3f }, { "fltType", 1 }, { "cutoff", 20000 },
            { "mod1Src", 7 }, { "mod1Dest", 1 }, { "mod1Amt", 0.45f }, { "mod2Src", 8 }, { "mod2Dest", 8 }, { "mod2Amt", -0.75f },
            { "mod3Src", 9 }, { "mod3Dest", 19 }, { "mod3Amt", 0.6f },
            { "distType", 0 }, { "distDrive", 0.5f }, { "distMix", 1 }, { "ott", 0.3f }, { "verbMix", 0.06f }, { "verbSize", 0.4f } },
          { "GRIT", "DARK", "DIRT", "MACRO 4" } },

        { "Midnight Log", Bass, { { "aTable", 12 }, { "aPos", 0.151f }, { "aOct", -1 }, { "aLevel", 0.9f }, { "aWarp", 6 }, { "aWarpAmt", 0.13f },
            { "bOn", 1 }, { "bLevel", 0.001f }, { "bFilter", 0 }, { "velSens", 0.6f },
            { "ampD", 0.55f }, { "ampS", 0 }, { "ampR", 0.5f }, { "modD", 0.03f }, { "modS", 0 },
            { "mod1Src", 3 }, { "mod1Dest", 3 }, { "mod1Amt", 0.85f }, { "mod2Src", 7 }, { "mod2Dest", 3 }, { "mod2Amt", 0.4f },
            { "fltType", 0 }, { "cutoff", 2500 }, { "chorusMix", 0.2f }, { "dlyTime", 5 }, { "dlyFb", 0.3f }, { "dlyTone", 0.35f }, { "dlyMix", 0.12f },
            { "verbMix", 0.12f }, { "verbSize", 0.55f } },
          { "KNOCK", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Golden Log Slide", Bass, { { "aTable", 12 }, { "aPos", 0.45f }, { "aOct", -1 }, { "aLevel", 0.9f }, { "aWarp", 6 }, { "aWarpAmt", 0.15f },
            { "bOn", 1 }, { "bLevel", 0.001f }, { "bFilter", 0 }, { "mode", 2 }, { "glide", 0.09f }, { "bendRange", 12 },
            { "ampD", 0.5f }, { "ampS", 0 }, { "ampR", 0.5f }, { "modD", 0.035f }, { "modS", 0 },
            { "mod1Src", 3 }, { "mod1Dest", 3 }, { "mod1Amt", 0.85f }, { "mod2Src", 7 }, { "mod2Dest", 3 }, { "mod2Amt", 0.4f },
            { "fltOn", 0 }, { "distType", 3 }, { "distDrive", 0.3f }, { "distMix", 0.8f }, { "ott", 0.2f } },
          { "KNOCK", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Galactic Reese", Bass, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.4f }, { "aBlend", 0.8f },
            { "bOn", 1 }, { "bTable", 14 }, { "bPos", 0.4f }, { "bOct", -1 }, { "bLevel", 0.4f },
            { "subOn", 1 }, { "subOct", -1 }, { "subLevel", 0.45f }, { "mode", 2 }, { "glide", 0.05f }, { "retrig", 0 },
            { "fltType", 7 }, { "cutoff", 400 }, { "res", 0.5f }, { "lfo1Rate", 0.12f }, { "lfo1Retrig", 0 },
            { "mod1Src", 1 }, { "mod1Dest", 8 }, { "mod1Amt", 0.45f }, { "mod2Src", 7 }, { "mod2Dest", 8 }, { "mod2Amt", 0.6f },
            { "ott", 0.5f }, { "distType", 0 }, { "distDrive", 0.3f }, { "distMix", 0.5f } },
          { "VOWEL", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Hyperdrive Growl", Bass, { { "aTable", 5 }, { "aPos", 0.3f }, { "aWarp", 6 }, { "aWarpAmt", 0.1f }, { "aUni", 3 }, { "aDetune", 0.12f },
            { "bOn", 1 }, { "bTable", 10 }, { "bPos", 0.4f }, { "bLevel", 0.05f }, { "bFilter", 0 },
            { "subOn", 1 }, { "subOct", -1 }, { "subLevel", 0.5f }, { "mode", 1 },
            { "fltType", 1 }, { "cutoff", 1400 }, { "res", 0.35f }, { "lfo1Shape", 1 }, { "lfo1Sync", 11 },
            { "mod1Src", 1 }, { "mod1Dest", 1 }, { "mod1Amt", 0.45f }, { "mod2Src", 1 }, { "mod2Dest", 8 }, { "mod2Amt", 0.35f },
            { "mod3Src", 7 }, { "mod3Dest", 3 }, { "mod3Amt", 0.5f }, { "mod4Src", 8 }, { "mod4Dest", 17 }, { "mod4Amt", 0.34f },
            { "ott", 0.7f }, { "distType", 2 }, { "distDrive", 0.3f }, { "distMix", 0.45f } },
          { "RAGE", "SPEED", "MACRO 3", "MACRO 4" } },

        { "Founders Stab", Lead, { { "chord", 14 }, { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 3 }, { "aDetune", 0.12f },
            { "bOn", 1 }, { "bTable", 2 }, { "bPos", 0.2f }, { "bLevel", 0.4f },
            { "ampD", 0.35f }, { "ampS", 0.15f }, { "ampR", 0.25f }, { "fltType", 1 }, { "cutoff", 1300 }, { "res", 0.25f }, { "fltEnv", 0.35f },
            { "modD", 0.25f }, { "modS", 0 }, { "mod1Src", 7 }, { "mod1Dest", 8 }, { "mod1Amt", 0.5f },
            { "dlyTime", 5 }, { "dlyFb", 0.4f }, { "dlyTone", 0.45f }, { "dlyMix", 0.18f }, { "verbMix", 0.2f }, { "verbSize", 0.55f } },
          { "OPEN", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Afterparty Organ", Lead, { { "chord", 11 }, { "aTable", 9 }, { "aPos", 0.15f }, { "bOn", 1 }, { "bTable", 9 }, { "bPos", 0.45f },
            { "bOct", 1 }, { "bLevel", 0.3f }, { "ampD", 0.4f }, { "ampS", 0.3f }, { "ampR", 0.2f }, { "fltType", 0 }, { "cutoff", 4500 },
            { "chorusMix", 0.4f }, { "verbMix", 0.2f }, { "verbSize", 0.5f } },
          { "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Warehouse Acid", Bass, { { "aTable", 0 }, { "aPos", 0.66f }, { "mode", 1 }, { "glide", 0.06f },
            { "ampD", 0.35f }, { "ampS", 0.5f }, { "ampR", 0.08f }, { "fltType", 5 }, { "cutoff", 300 }, { "res", 0.9f }, { "fltEnv", 0.6f },
            { "modD", 0.13f }, { "modS", 0 }, { "mod1Src", 7 }, { "mod1Dest", 8 }, { "mod1Amt", 0.6f }, { "mod2Src", 4 }, { "mod2Dest", 8 }, { "mod2Amt", 0.35f },
            { "distType", 0 }, { "distDrive", 0.5f }, { "distMix", 0.7f }, { "dlyTime", 5 }, { "dlyFb", 0.45f }, { "dlyMix", 0.18f },
            { "verbMix", 0.15f }, { "verbSize", 0.7f } },
          { "CUTOFF", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Euro Anthem", Lead, { { "mode", 0 }, { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.45f }, { "aBlend", 0.85f },
            { "bOn", 1 }, { "bTable", 2 }, { "bPos", 0.3f }, { "bOct", 1 }, { "bUni", 5 }, { "bDetune", 0.3f }, { "bLevel", 0.35f },
            { "ampS", 0.9f }, { "ampR", 0.4f }, { "fltType", 1 }, { "cutoff", 6000 },
            { "dlyTime", 5 }, { "dlyFb", 0.4f }, { "dlyMix", 0.2f }, { "verbMix", 0.3f }, { "verbSize", 0.7f }, { "width", 1.3f } },
          { "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Siren Command", Lead, { { "aTable", 2 }, { "aPos", 0.05f }, { "mode", 1 }, { "retrig", 0 }, { "fltType", 0 }, { "cutoff", 3200 },
            { "lfo1Shape", 1 }, { "lfo1Rate", 3.0f }, { "mod1Src", 1 }, { "mod1Dest", 7 }, { "mod1Amt", 0.6f },
            { "mod2Src", 7 }, { "mod2Dest", 17 }, { "mod2Amt", 0.4f },
            { "dlyTime", 5 }, { "dlyFb", 0.8f }, { "dlyTone", 0.35f }, { "dlyMix", 0.5f }, { "verbMix", 0.3f }, { "verbSize", 0.65f }, { "verbShimmer", 0.25f } },
          { "SPEED", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Dancehall Laser", Lead, { { "aTable", 12 }, { "aPos", 0.3f }, { "aWarp", 6 }, { "aWarpAmt", 0.2f },
            { "bOn", 1 }, { "bOct", 1 }, { "bLevel", 0.001f }, { "bFilter", 0 }, { "dropAmt", 36 }, { "dropTime", 0.12f },
            { "ampD", 0.3f }, { "ampS", 0 }, { "ampR", 0.2f }, { "fltOn", 0 },
            { "dlyTime", 4 }, { "dlyFb", 0.55f }, { "dlyMix", 0.35f }, { "verbMix", 0.2f } },
          { "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Supernova Pad", Pad, { { "aTable", 14 }, { "aPos", 0.6f }, { "aUni", 7 }, { "aDetune", 0.3f }, { "aBlend", 0.85f },
            { "bOn", 1 }, { "bTable", 13 }, { "bPos", 0.5f }, { "bOct", 1 }, { "bLevel", 0.35f }, { "retrig", 0 }, { "drift", 0.4f },
            { "ampA", 2.0f }, { "ampD", 4.0f }, { "ampS", 0.85f }, { "ampR", 6.0f }, { "fltType", 0 }, { "cutoff", 3000 },
            { "lfo1Rate", 0.05f }, { "lfo1Retrig", 0 }, { "lfo2Shape", 6 }, { "lfo2Rate", 0.15f }, { "lfo2Retrig", 0 },
            { "mod1Src", 1 }, { "mod1Dest", 8 }, { "mod1Amt", 0.3f }, { "mod2Src", 2 }, { "mod2Dest", 1 }, { "mod2Amt", 0.3f },
            { "mod3Src", 7 }, { "mod3Dest", 24 }, { "mod3Amt", 0.5f },
            { "chorusMix", 0.35f }, { "verbSize", 1.0f }, { "verbMix", 0.6f }, { "verbShimmer", 0.65f }, { "width", 1.4f } },
          { "SHIMMER", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Event Horizon Keys", Pad, { { "chord", 11 }, { "strum", 0.05f }, { "aTable", 12 }, { "aPos", 0.2f }, { "aWarp", 6 }, { "aWarpAmt", 0.06f },
            { "bOn", 1 }, { "bTable", 13 }, { "bPos", 0.3f }, { "bOct", 1 }, { "bLevel", 0.002f }, { "bFilter", 0 },
            { "ampD", 3.0f }, { "ampS", 0 }, { "ampR", 3.0f }, { "modD", 0.4f }, { "modS", 0 },
            { "mod1Src", 3 }, { "mod1Dest", 3 }, { "mod1Amt", 0.3f }, { "fltOn", 0 },
            { "verbSize", 0.85f }, { "verbMix", 0.45f }, { "verbShimmer", 0.55f }, { "chorusMix", 0.3f } },
          { "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Solo Flight", Lead, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 3 }, { "aDetune", 0.07f },
            { "bOn", 1 }, { "bTable", 1 }, { "bPos", 0.35f }, { "bLevel", 0.4f }, { "mode", 2 }, { "glide", 0.14f }, { "retrig", 0 },
            { "ampA", 0.005f }, { "ampS", 0.85f }, { "ampR", 0.45f }, { "fltType", 1 }, { "cutoff", 2400 }, { "res", 0.25f },
            { "fltEnv", 0.3f }, { "modD", 0.6f }, { "modS", 0.3f },
            { "lfo2Rate", 5.5f }, { "lfo2Fade", 0.6f }, { "mod1Src", 2 }, { "mod1Dest", 7 }, { "mod1Amt", 0.015f },
            { "distType", 0 }, { "distDrive", 0.35f }, { "distMix", 0.5f },
            { "dlyTime", 8 }, { "dlyFb", 0.45f }, { "dlyMix", 0.3f }, { "verbSize", 0.75f }, { "verbMix", 0.35f } },
          { "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Cosmic Arp", Lead, { { "arpOn", 1 }, { "arpMode", 2 }, { "arpRate", 3 }, { "arpOct", 3 }, { "arpGate", 0.45f },
            { "aTable", 13 }, { "aPos", 0.35f }, { "ampD", 0.3f }, { "ampS", 0 }, { "ampR", 0.3f }, { "fltType", 1 }, { "cutoff", 3000 },
            { "dlyTime", 5 }, { "dlyFb", 0.45f }, { "dlyMix", 0.25f }, { "verbMix", 0.3f }, { "verbSize", 0.8f }, { "verbShimmer", 0.35f } },
          { "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Black Hole Sub", Bass, { { "aTable", 0 }, { "aPos", 0.05f }, { "mode", 2 }, { "glide", 0.06f }, { "drift", 0.2f },
            { "ampD", 2.0f }, { "ampS", 0.8f }, { "ampR", 0.2f }, { "fltOn", 0 },
            { "distType", 0 }, { "distDrive", 0.25f }, { "distMix", 0.5f } },
          { "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Glass Rain", Pad, { { "arpOn", 1 }, { "arpMode", 3 }, { "arpRate", 4 }, { "arpOct", 3 }, { "arpGate", 0.3f },
            { "aTable", 13 }, { "aPos", 0.7f }, { "ampD", 0.5f }, { "ampS", 0 }, { "ampR", 0.9f }, { "fltOn", 0 },
            { "dlyTime", 5 }, { "dlyFb", 0.5f }, { "dlyMix", 0.3f }, { "verbMix", 0.45f }, { "verbSize", 0.9f }, { "verbShimmer", 0.6f } },
          { "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Rodeo Night 808", Bass, { { "aTable", 3 }, { "aPos", 0.65f }, { "dropAmt", 12 }, { "dropTime", 0.04f }, { "mode", 2 }, { "glide", 0.12f },
            { "bendRange", 12 }, { "ampD", 3.5f }, { "ampS", 0.7f }, { "ampR", 0.3f }, { "fltType", 1 }, { "cutoff", 2500 }, { "fltKey", 0.5f },
            { "mod1Src", 7 }, { "mod1Dest", 8 }, { "mod1Amt", 0.7f }, { "distType", 2 }, { "distDrive", 0.35f }, { "distMix", 0.6f }, { "ott", 0.3f } },
          { "OPEN", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Amapiano Keys Stab", Lead, { { "chord", 9 }, { "aTable", 12 }, { "aPos", 0.35f }, { "aWarp", 6 }, { "aWarpAmt", 0.05f },
            { "bOn", 1 }, { "bOct", 1 }, { "bLevel", 0.001f }, { "bFilter", 0 },
            { "ampD", 1.2f }, { "ampS", 0.05f }, { "ampR", 0.5f }, { "modD", 0.3f }, { "modS", 0 },
            { "mod1Src", 3 }, { "mod1Dest", 3 }, { "mod1Amt", 0.3f }, { "fltType", 0 }, { "cutoff", 3500 },
            { "chorusMix", 0.3f }, { "verbMix", 0.2f }, { "verbSize", 0.5f } },
          { "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Techno Cathedral", Pad, { { "chord", 15 }, { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 3 }, { "aDetune", 0.1f },
            { "bOn", 1 }, { "bTable", 2 }, { "bPos", 0.3f }, { "bLevel", 0.35f }, { "ampD", 0.4f }, { "ampS", 0.1f }, { "ampR", 0.3f },
            { "fltType", 1 }, { "cutoff", 800 }, { "res", 0.3f }, { "fltEnv", 0.3f }, { "modD", 0.25f }, { "modS", 0 },
            { "dlyTime", 5 }, { "dlyFb", 0.65f }, { "dlyTone", 0.3f }, { "dlyMix", 0.35f }, { "verbSize", 1.0f }, { "verbMix", 0.5f } },
          { "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Talkbox Bass", Bass, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 2 }, { "aDetune", 0.08f },
            { "subOn", 1 }, { "subOct", -1 }, { "subLevel", 0.5f }, { "mode", 1 }, { "glide", 0.04f },
            { "fltType", 7 }, { "cutoff", 300 }, { "res", 0.6f }, { "lfo1Shape", 6 }, { "lfo1Sync", 7 },
            { "mod1Src", 1 }, { "mod1Dest", 8 }, { "mod1Amt", 0.7f }, { "ott", 0.4f },
            { "distType", 0 }, { "distDrive", 0.3f }, { "distMix", 0.4f } },
          { "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4" } },

        { "Neon Drift Pad", Pad, { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.3f }, { "aBlend", 0.8f },
            { "retrig", 0 }, { "drift", 0.5f }, { "ampA", 1.5f }, { "ampS", 0.85f }, { "ampR", 4.0f },
            { "fltType", 7 }, { "cutoff", 500 }, { "res", 0.45f }, { "lfo1Rate", 0.06f }, { "lfo1Retrig", 0 },
            { "mod1Src", 1 }, { "mod1Dest", 8 }, { "mod1Amt", 0.6f },
            { "chorusMix", 0.4f }, { "verbSize", 0.9f }, { "verbMix", 0.5f }, { "verbShimmer", 0.3f }, { "width", 1.3f } },
          { "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4" } },
    };

    HypernovaAudioProcessor proc;
    Head head;
    proc.setPlayHead (&head);
    proc.prepareToPlay (48000.0, 256);

    for (const auto& s : pack)
    {
        proc.applyPresetValues (s.values);
        for (int m = 0; m < 4; ++m) proc.setMacroName (m, s.macros[(size_t) m]);
        proc.setParam ("volume", 0.0f);
        const float db = measure (proc, head, s.kind);
        const float target = s.kind == Bass ? -10.0f : s.kind == Lead ? -12.5f : -14.5f;
        proc.setParam ("volume", juce::jlimit (-30.0f, 12.0f, target - db));
        const auto file = outDir.getChildFile (juce::String (s.name) + HypernovaAudioProcessor::presetExtension);
        if (! proc.writePresetFile (file, s.name, "Hypernova Founders"))
        {
            std::printf ("FAILED %s\n", s.name);
            return 1;
        }
        std::printf ("%-22s %6.1f dB -> volume %+.1f\n", s.name, db, target - db);
    }
    std::printf ("%d sounds written to %s\n", (int) pack.size(), outDir.getFullPathName().toRawUTF8());
    return 0;
}
