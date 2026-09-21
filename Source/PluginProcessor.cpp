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

    return l;
}

//==============================================================================
HypernovaAudioProcessor::HypernovaAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "ArrowBass", createLayout()) // state tag kept from the Arrow Bass days so old sessions load
{
    WavetableBank::get(); // build the tables up front, not on the audio thread
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            raw[rp->getParameterID().toStdString()] = apvts.getRawParameterValue (rp->getParameterID());
}

float HypernovaAudioProcessor::param (const char* id) const
{
    auto it = raw.find (id);
    jassert (it != raw.end());
    return it != raw.end() ? it->second->load (std::memory_order_relaxed) : 0.0f;
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

void HypernovaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateNow = sampleRate;
    maxBlock = juce::jmax (32, samplesPerBlock);
    for (int i = 0; i < 2; ++i)
    {
        voiceOversampler[(size_t) i] = std::make_unique<juce::dsp::Oversampling<float>> (2, (size_t) (i + 1),
                                           juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        voiceOversampler[(size_t) i]->initProcessing ((size_t) maxBlock);
    }
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
    for (int o = 0; o < 2; ++o)
    {
        const std::string p = o == 0 ? "a" : "b";
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
    }
    s.subOn = param ("subOn") > 0.5f;
    s.subShape = (int) param ("subShape");
    s.subOct = (int) param ("subOct");
    s.subLevel = param ("subLevel");
    s.subToFilter = param ("subFilter") > 0.5f;
    s.noiseLevel = param ("noiseLevel");
    s.noiseTone = param ("noiseTone");
    s.noiseType = (int) param ("noiseType");
    s.noiseToFilter = param ("noiseFilter") > 0.5f;
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
    for (int i = 0; i < 2; ++i)
    {
        const std::string p = "lfo" + std::to_string (i + 1);
        auto& ls = s.lfo[(size_t) i];
        ls.shape = (int) param ((p + "Shape").c_str());
        const int sync = (int) param ((p + "Sync").c_str());
        ls.rateHz = sync == 0 ? param ((p + "Rate").c_str()) : (float) (bpm / 60.0 / lfoSyncBeats (sync));
        ls.retrig = param ((p + "Retrig").c_str()) > 0.5f;
        ls.fade = param ((p + "Fade").c_str());
    }
    for (int i = 0; i < NumModSlots; ++i)
    {
        const std::string p = "mod" + std::to_string (i + 1);
        s.mod[(size_t) i] = { (int) param ((p + "Src").c_str()), (int) param ((p + "Dest").c_str()), param ((p + "Amt").c_str()) };
    }
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
    for (int o = 0; o < 2; ++o)
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
    f.distOn = param ("distOn") > 0.5f;
    f.ottOn = param ("ottOn") > 0.5f;
    f.chorusOn = param ("chorusOn") > 0.5f;
    f.delayOn = param ("dlyOn") > 0.5f;
    f.reverbOn = param ("verbOn") > 0.5f;
    f.eqOn = param ("eqOn") > 0.5f;
    f.eqLow = param ("eqLow");
    f.eqHigh = param ("eqHigh");
    f.bpm = bpm;
    return f;
}

//==============================================================================
void HypernovaAudioProcessor::noteOn (int note, float velocity)
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

void HypernovaAudioProcessor::noteOff (int note)
{
    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
    if (blockSettings.mode == ModePoly)
    {
        for (size_t i = 0; i < voices.size(); ++i)
        {
            auto& v = voices[i];
            if (v.trigger == note && v.held)
            {
                if (sustainPedal) { v.held = false; sustained[i] = true; }
                else v.stop();
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
        v.stop();
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

    blockSettings = readSynthSettings();
    applyQuality ((int) param ("quality"));
    smoothSettings (blockSettings, numSamples);
    auto& settings = blockSettings;
    if (settings.mode != lastMode)
    {
        if (lastMode >= 0) allNotesOff (false);
        lastMode = settings.mode;
    }

    for (int i = 0; i < 2; ++i)
    {
        const int sync = (int) param (i == 0 ? "lfo1Sync" : "lfo2Sync");
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
    const int f = osFactor;
    auto renderVoices = [&] (int from, int to)
    {
        if (to <= from) return;
        for (auto& v : voices)
            v.render (vL + from * f, vR + from * f, (to - from) * f, settings, globalMod);
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

        if (msg.isNoteOn())
            noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            noteOff (msg.getNoteNumber());
        else if (msg.isPitchWheel())
            pitchWheel = msg.getPitchWheelValue();
        else if (msg.isController())
        {
            if (msg.getControllerNumber() == 1)
                globalMod.modWheel = (float) msg.getControllerValue() / 127.0f;
            else if (msg.getControllerNumber() == 64)
            {
                sustainPedal = msg.getControllerValue() >= 64;
                if (! sustainPedal)
                    for (size_t i = 0; i < voices.size(); ++i)
                        if (sustained[i]) { sustained[i] = false; if (! voices[i].held) voices[i].stop(); }
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

    // Master effects in chunks the effects were prepared for.
    auto fx = readFxSettings();
    applyGlobalModulation (fx);
    for (int start = 0; start < numSamples; start += maxBlock)
    {
        const int n = juce::jmin (maxBlock, numSamples - start);
        float* chans[2] = { L + start, R + start };
        juce::AudioBuffer<float> chunk (chans, 2, n);
        effects.process (chunk, fx);
    }

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
        for (int o = 0; o < 2; ++o)
        {
            shownPos[o] = newest->shownPos[o];
            shownLfo[o] = newest->shownLfo[o];
            shownLfoPhase[o] = (float) newest->shownLfoPhase[o];
        }
        shownEnv = newest->ampLevel();
        shownCutoff = newest->shownCutoff;
        shownNote = newest->note;
    }
    else
    {
        for (int o = 0; o < 2; ++o)
        {
            shownPos[o] = settings.osc[(size_t) o].pos;
            shownLfoPhase[o] = (float) globalMod.lfoPhase[(size_t) o];
            shownLfo[o] = dsp::lfoShape (settings.lfo[(size_t) o].shape, globalMod.lfoPhase[(size_t) o], 0, 0);
        }
        shownEnv = 0;
        shownCutoff = settings.cutoff;
        shownNote = -1;
    }
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
    for (const auto& slot : blockSettings.mod)
    {
        if (slot.dest < FirstGlobalDest || slot.dest >= NumDest || slot.src == SrcNone) continue;
        float v = 0;
        if (slot.src >= SrcMacro1 && slot.src <= SrcMacro4) v = globalMod.macros[(size_t) (slot.src - SrcMacro1)];
        else if (slot.src == SrcModWheel) v = globalMod.modWheel;
        else if (newest != nullptr) v = newest->lastSrc[slot.src];
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
    if (juce::isPositiveAndBelow (osc, 2))
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
    if (! juce::isPositiveAndBelow (osc, 2)) return false;
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
    apvts.replaceState (state);
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
    }
    return count;
}

void HypernovaAudioProcessor::stepPreset (int delta)
{
    const int n = (int) factoryPresets().size();
    loadFactoryPreset ((currentProgram + delta + n) % n);
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
        setParam ("lfo1Shape", (float) r.nextInt (NumLfoShapes));
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
    for (int o = 0; o < 2; ++o)
        state.setProperty (o == 0 ? "aUserTable" : "bUserTable", userTableSlot[(size_t) o], nullptr);
    state.setProperty ("uiAnimation", uiAnimation.load(), nullptr);
    state.setProperty ("uiScale", uiScalePercent.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void HypernovaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.hasType (apvts.state.getType())) return;
        apvts.replaceState (state);
        const juce::ScopedLock sl (nameLock);
        presetName = state.getProperty ("presetName", "Init").toString();
        presetCategory = state.getProperty ("presetCategory", "").toString();
        for (int i = 0; i < 4; ++i)
            macroNames[(size_t) i] = state.getProperty ("macro" + juce::String (i + 1) + "Name", "MACRO " + juce::String (i + 1)).toString();
        currentProgram = state.getProperty ("program", 0);
        for (int o = 0; o < 2; ++o)
            setUserTable (o, state.getProperty (o == 0 ? "aUserTable" : "bUserTable", "").toString());
        uiAnimation = (int) state.getProperty ("uiAnimation", 0);
        uiScalePercent = (int) state.getProperty ("uiScale", 100);
    }
    ++presetVersion;
}

juce::AudioProcessorEditor* HypernovaAudioProcessor::createEditor()
{
    return new HypernovaAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HypernovaAudioProcessor();
}
