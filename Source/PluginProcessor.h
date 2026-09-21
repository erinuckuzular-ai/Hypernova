#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "DSP/Synth.h"
#include "DSP/Effects.h"
#include "Presets.h"
#include <unordered_map>

namespace ab
{
    inline juce::StringArray lfoSyncNames()
    {
        return { "Free", "8 bars", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4 T", "1/8 T", "1/16 T", "1/4 D", "1/8 D" };
    }
    inline juce::StringArray chordNames()
    {
        return { "Off", "Octave", "Fifth", "Power", "Major", "Minor", "Sus2", "Sus4", "Maj7", "Min7", "Dom7", "Min9", "Maj9", "Min11",
                 "House Min7", "Open Minor", "Dim7" };
    }
    inline const std::vector<int>& chordIntervals (int i)
    {
        static const std::vector<std::vector<int>> c { { 0 }, { 0, 12 }, { 0, 7 }, { 0, 7, 12 }, { 0, 4, 7 }, { 0, 3, 7 }, { 0, 2, 7 }, { 0, 5, 7 },
            { 0, 4, 7, 11 }, { 0, 3, 7, 10 }, { 0, 4, 7, 10 }, { 0, 3, 7, 10, 14 }, { 0, 4, 7, 11, 14 }, { 0, 3, 7, 10, 14, 17 },
            { 0, 7, 10, 15 }, { 0, 7, 15 }, { 0, 3, 6, 9 } };
        return c[(size_t) juce::jlimit (0, (int) c.size() - 1, i)];
    }
    inline juce::StringArray arpModeNames() { return { "Up", "Down", "Up/Down", "Random", "As Played" }; }
    inline juce::StringArray arpRateNames() { return { "1/4", "1/8", "1/8 T", "1/16", "1/16 T", "1/32" }; }
    inline double arpRateBeats (int i)
    {
        static const double b[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        return b[juce::jlimit (0, 5, i)];
    }

    inline double lfoSyncBeats (int i)
    {
        static const double b[] = { 0, 32, 16, 8, 4, 2, 1, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.5, 0.75 };
        return b[juce::jlimit (0, 14, i)];
    }

    // Lock-free-ish ring of the output for the visualisers. Torn reads only cost a pixel.
    struct ScopeRing
    {
        static constexpr int size = 8192;
        std::array<float, size> l {}, r {};
        std::atomic<int> write { 0 };

        void push (const float* L, const float* R, int n)
        {
            int w = write.load (std::memory_order_relaxed);
            for (int i = 0; i < n; ++i)
            {
                l[(size_t) w] = L[i];
                r[(size_t) w] = R[i];
                w = (w + 1) & (size - 1);
            }
            write.store (w, std::memory_order_release);
        }

        // Copies the newest n samples, oldest first.
        void latest (float* outL, float* outR, int n) const
        {
            const int w = write.load (std::memory_order_acquire);
            for (int i = 0; i < n; ++i)
            {
                const int idx = (w - n + i + size) & (size - 1);
                outL[i] = l[(size_t) idx];
                if (outR != nullptr) outR[i] = r[(size_t) idx];
            }
        }
    };
}

class HypernovaAudioProcessor  : public juce::AudioProcessor
{
public:
    HypernovaAudioProcessor();
    ~HypernovaAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Hypernova"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return (int) ab::factoryPresets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override { loadFactoryPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    void loadFactoryPreset (int index);
    bool loadUserPreset (const juce::File&);
    bool saveUserPreset (const juce::String& name, const juce::String& author = {});
    bool exportPreset (const juce::File&, const juce::String& author = {});
    bool writePresetFile (const juce::File&, const juce::String& name, const juce::String& author);
    int importPresets (const juce::Array<juce::File>& filesOrFolders, juce::File* firstImported = nullptr);
    static constexpr const char* presetExtension = ".hnpreset";
    static bool isPresetFile (const juce::File&);
    static juce::Array<juce::File> presetFilesIn (const juce::File& folder);
    static juce::File packFolder();
    void stepPreset (int delta);
    void randomize();
    void panic() { panicRequested = true; }

    static juce::File userPresetFolder();
    static juce::Array<juce::File> userPresetFiles();

    juce::String getPresetName() const { const juce::ScopedLock sl (nameLock); return presetName; }
    juce::String getPresetCategory() const { const juce::ScopedLock sl (nameLock); return presetCategory; }
    juce::String getMacroName (int i) const { const juce::ScopedLock sl (nameLock); return macroNames[(size_t) i]; }
    void setMacroName (int i, const juce::String& n);
    std::atomic<int> presetVersion { 0 };

    //==========================================================================
    // Imported wavetables. Tables load on the message thread into a cache that is never emptied while the
    // plug-in lives, so the audio thread can hold a plain pointer to one safely.
    static juce::File wavetableFolder();
    juce::String importWavetable (const juce::File& audioOrTableFile, int osc, juce::String& error);
    bool setUserTable (int osc, const juce::String& name); // "" restores the factory table
    juce::String userTableName (int osc) const { return userTableSlot[(size_t) osc]; }
    const ab::Wavetable* currentUserTable (int osc) const { return userTable[(size_t) osc].load(); }
    static juce::StringArray installedWavetables();
    std::atomic<int> tableVersion { 0 };
    int uiDeckPage = 0; // which tab of the editor's bottom deck is showing
    std::atomic<int> uiAnimation { 0 }; // backdrop animation: 0 full, 1 calm, 2 off (saved with the session)
    std::atomic<int> uiScalePercent { 100 };

    juce::UndoManager undoManager { 30000, 60 }; // declared before apvts, which records into it
    juce::AudioProcessorValueTreeState apvts;
    void mutate (float amount); // nudge the current sound by up to `amount` of each control's range
    juce::MidiKeyboardState keyboardState;

    // Visualiser taps.
    ab::ScopeRing scope;
    std::atomic<float> shownPos[2] {}, shownLfo[2] {}, shownLfoPhase[2] {}, shownEnv { 0 }, shownCutoff { 1000 };
    std::atomic<int> shownVoices { 0 }, shownNote { -1 };
    double getCurrentSampleRate() const { return sampleRateNow; }
    bool isAsleep() const { return sleeping; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParam (const juce::String& id, float realValue);
    void applyPresetValues (const std::vector<std::pair<const char*, float>>& values);

private:
    ab::SynthSettings readSynthSettings();
    ab::FxSettings readFxSettings();
    float param (const char* id) const;

    void processChunk (juce::AudioBuffer<float>&, juce::MidiBuffer&);
    void applyQuality (int q);
    void smoothSettings (ab::SynthSettings&, int numSamples);
    void noteOn (int note, float velocity);
    ab::Voice& allocateVoice();
    void runArpeggiator (juce::MidiBuffer& midi, int numSamples, double ppq, bool playing);
    void applyGlobalModulation (ab::FxSettings& fx);
    void noteOff (int note);
    void allNotesOff (bool hard);

    std::array<ab::Voice, ab::MaxVoices> voices;
    ab::Effects effects;
    ab::GlobalMod globalMod;
    ab::SynthSettings blockSettings, smoothed;
    bool smoothReady = false;
    int smoothVersion = -1;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 2> voiceOversampler; // 2x, 4x
    int osFactor = 1, currentQuality = -1;
    // Look-ahead peak limiter: audio is delayed limiterLen samples so gain can ramp down before a peak
    // instead of slamming onto it (an instant gain step is itself a click).
    float limiterGain = 1.0f, limiterOutGain = 1.0f;
    int limiterLen = 1, limiterPos = 0, limiterLoud = 0;
    double limiterSum = 0;
    std::vector<float> limTarget, limHeld, limDelayL, limDelayR;
    bool sleeping = false;
    int silentSamples = 0;
    std::vector<int> heldNotes;
    std::vector<juce::MidiMessageMetadata> midiOrder;
    // Arpeggiator
    std::vector<int> arpKeys;          // held keys in the order they were pressed
    std::array<float, 128> arpVelocity {};
    juce::MidiBuffer arpOut;
    double arpBeat = 0, arpOffBeat = -1;
    int arpStep = 0, arpNote = -1;
    bool arpWasOn = false;
    juce::Random arpRandom;
    std::array<bool, ab::MaxVoices> sustained {};
    bool sustainPedal = false;
    int lastNote = -1;
    int monoVoice = 0; // the voice currently playing in Mono/Legato
    juce::uint64 noteCounter = 0;
    double sampleRateNow = 44100.0;
    int maxBlock = 512;
    double freeLfoPhase[2] {};
    float bpm = 120.0f;
    int pitchWheel = 8192;
    std::atomic<bool> panicRequested { false };
    std::unordered_map<std::string, std::shared_ptr<const ab::Wavetable>> tableCache;
    std::array<std::atomic<const ab::Wavetable*>, 2> userTable { nullptr, nullptr };
    std::array<juce::String, 2> userTableSlot;
    juce::SmoothedValue<float> masterGain;
    int lastMode = -1;

    std::unordered_map<std::string, std::atomic<float>*> raw;

    juce::CriticalSection nameLock;
    juce::String presetName { "Init" }, presetCategory { "Init" };
    std::array<juce::String, 4> macroNames { "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4" };
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HypernovaAudioProcessor)
};
