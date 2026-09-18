#pragma once

#include "Wavetables.h"
#include <atomic>

// Voice engine: two unison wavetable oscillators, sub, noise, pitch drop, glide, filter, two envelopes,
// two LFOs and a six-slot mod matrix.
namespace ab
{

enum Warp { WarpOff, WarpSync, WarpBend, WarpMirror, WarpPwm, WarpCrush, WarpFm, NumWarps };
inline juce::StringArray warpNames() { return { "Off", "Sync", "Bend", "Mirror", "PWM", "Crush", "FM" }; }

enum FilterType { FLp12, FLp24, FHp12, FBp, FNotch, FDirty, FComb, FFormant, NumFilterTypes }; // append only
inline juce::StringArray filterNames() { return { "Low Pass 12", "Low Pass 24", "High Pass", "Band Pass", "Notch", "Dirty LP", "Comb", "Formant" }; }

enum SubShape { SubSine, SubTri, SubSquare, SubPunch, NumSubShapes };
inline juce::StringArray subShapeNames() { return { "Sine", "Triangle", "Square", "Punch" }; }

enum LfoShape { LSine, LTri, LSaw, LRamp, LSquare, LSnH, LSmooth, NumLfoShapes };
inline juce::StringArray lfoShapeNames() { return { "Sine", "Triangle", "Saw Down", "Ramp Up", "Square", "Sample & Hold", "Smooth Random" }; }

enum VoiceMode { ModePoly, ModeMono, ModeLegato, NumModes };
inline juce::StringArray voiceModeNames() { return { "Poly", "Mono", "Legato" }; }

enum ModSrc { SrcNone, SrcLfo1, SrcLfo2, SrcEnv2, SrcVelocity, SrcModWheel, SrcNote, SrcMacro1, SrcMacro2, SrcMacro3, SrcMacro4, SrcRandom, NumSrc };
inline juce::StringArray modSrcNames()
{
    return { "-", "LFO 1", "LFO 2", "Mod Env", "Velocity", "Mod Wheel", "Note", "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Random" };
}

// Append only (saved sessions store indices). Everything from DDistFx on is global: the processor applies it to the effects.
enum ModDest { DNone, DAPos, DBPos, DAWarp, DBWarp, DALevel, DBLevel, DPitch, DCutoff, DRes, DDrive, DSub, DNoise, DADetune, DBDetune, DPan, DAmp,
               DLfo1Rate, DLfo2Rate, DDistFx, DOttFx, DChorusFx, DDelayFx, DReverbFx, DShimmerFx, NumDest };
constexpr int FirstGlobalDest = DDistFx;
inline juce::StringArray modDestNames()
{
    return { "-", "A Position", "B Position", "A Warp", "B Warp", "A Level", "B Level", "Pitch", "Cutoff", "Resonance",
             "Filter Drive", "Sub Level", "Noise Level", "A Detune", "B Detune", "Pan", "Volume",
             "LFO 1 Rate", "LFO 2 Rate", "FX Distortion", "FX OTT", "FX Chorus", "FX Delay", "FX Reverb", "FX Shimmer" };
}

constexpr int NumModSlots = 8;
constexpr int MaxUnison = 7;
constexpr int MaxVoices = 16;
constexpr int SubBlock = 16;

//==============================================================================
struct OscSettings
{
    bool on = true, toFilter = true;
    int table = 0, warp = WarpOff, unison = 1;
    float pos = 0, warpAmt = 0, detune = 0.2f, blend = 0.5f, level = 0.8f, pan = 0, pitch = 0, width = 1.0f; // pitch in semitones
};

struct EnvSettings { float a = 0.001f, d = 0.3f, s = 1.0f, r = 0.2f; };

struct LfoSettings { int shape = LSine; float rateHz = 1.0f, fade = 0; bool retrig = true; };

struct ModSlot { int src = SrcNone, dest = DNone; float amount = 0; };

struct SynthSettings
{
    std::array<OscSettings, 2> osc;
    bool subOn = false, subToFilter = false;
    int subShape = SubSine, subOct = -1;
    float subLevel = 0.6f;
    float noiseLevel = 0, noiseTone = 0.7f;
    bool noiseToFilter = true;

    float pitchEnvAmt = 0, pitchEnvDecay = 0.05f;
    float glide = 0;
    int mode = ModePoly;
    bool phaseRetrig = true;

    bool filterOn = true;
    int filterType = FLp24;
    float cutoff = 18000, res = 0.1f, filterDrive = 0, filterEnv = 0, keytrack = 0, filterMix = 1.0f;

    std::array<EnvSettings, 2> env;
    std::array<LfoSettings, 2> lfo;
    std::array<ModSlot, NumModSlots> mod;
    float velSens = 0.5f;
    float transpose = 0, drift = 0; // semitones (incl. fine tune), 0..1
};

struct GlobalMod
{
    float modWheel = 0, bendSemis = 0;
    std::array<float, 4> macros {};
    std::array<double, 2> lfoPhase {}; // free/host-locked phase at block start, used by non-retriggered LFOs
};

//==============================================================================
namespace dsp
{
    inline double frac (double v) { return v - std::floor (v); }

    inline float polyBlep (double t, double dt)
    {
        if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    // Zavalishin TPT state-variable filter.
    struct SVF
    {
        float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
        void set (float g, float kk)
        {
            k = kk;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
        // Returns low, writes band/high.
        inline void tick (float v0, float& low, float& band, float& high)
        {
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            low = v2; band = v1; high = v0 - k * v1 - v2;
        }
        void reset() { ic1 = ic2 = 0; }
    };

    // Attack is linear, decay/release are exponential (times are to -60 dB), so bass envelopes snap.
    struct Env
    {
        enum Stage { Idle, Attack, Decay, Sustain, Release };
        Stage stage = Idle;
        float value = 0, attInc = 1, decCoef = 0, relCoef = 0, sus = 1;

        void set (const EnvSettings& e, double sr)
        {
            attInc = (float) (1.0 / juce::jmax (1.0, e.a * sr));
            decCoef = (float) std::exp (-6.9078 / juce::jmax (1.0, e.d * sr));
            relCoef = (float) std::exp (-6.9078 / juce::jmax (1.0, e.r * sr));
            sus = e.s;
        }
        void noteOn() { stage = Attack; }
        void noteOff() { if (stage != Idle) stage = Release; }
        void kill() { stage = Idle; value = 0; }
        bool active() const { return stage != Idle; }

        inline float tick()
        {
            switch (stage)
            {
                case Attack:
                    value += attInc;
                    if (value >= 1.0f) { value = 1.0f; stage = Decay; }
                    break;
                case Decay:
                    value = sus + (value - sus) * decCoef;
                    if (std::abs (value - sus) < 1.0e-4f) { value = sus; stage = Sustain; }
                    break;
                case Sustain:
                    value = sus;
                    if (sus <= 0.0f) { stage = Idle; value = 0; }
                    break;
                case Release:
                    value *= relCoef;
                    if (value < 1.0e-5f) { value = 0; stage = Idle; }
                    break;
                case Idle: break;
            }
            return value;
        }
    };

    struct Rng
    {
        juce::uint32 s = 0x9e3779b9u;
        inline float next() // -1..1
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return (float) s * (2.0f / 4294967295.0f) - 1.0f;
        }
    };

    inline float lfoShape (int shape, double p, float held, float prevHeld)
    {
        switch (shape)
        {
            case LSine:   return (float) std::sin (juce::MathConstants<double>::twoPi * p);
            case LTri:    return (float) (p < 0.25 ? 4.0 * p : (p < 0.75 ? 2.0 - 4.0 * p : 4.0 * p - 4.0));
            case LSaw:    return (float) (1.0 - 2.0 * p);
            case LRamp:   return (float) (2.0 * p - 1.0);
            case LSquare: return p < 0.5 ? 1.0f : -1.0f;
            case LSnH:    return held;
            case LSmooth:
            {
                const float s = (float) (p * p * (3.0 - 2.0 * p));
                return prevHeld + (held - prevHeld) * s;
            }
            default: return 0.0f;
        }
    }

    inline double warpPhase (int warp, double p, float amt, float crushSteps, float fm)
    {
        switch (warp)
        {
            case WarpSync:  return frac (p * (1.0 + amt * 7.0));
            case WarpBend:
            {
                const double k = 1.0 + amt * 7.0;
                return p * k / (p * (k - 1.0) + 1.0);
            }
            case WarpMirror:
            {
                const double tri = p < 0.5 ? 2.0 * p : 2.0 - 2.0 * p;
                return juce::jlimit (0.0, 0.999999, p + amt * (tri - p));
            }
            case WarpPwm:
            {
                const double d = 0.5 - 0.47 * amt;
                return p < d ? p * 0.5 / d : 0.5 + (p - d) * 0.5 / (1.0 - d);
            }
            case WarpCrush: return std::floor (p * crushSteps) / crushSteps;
            case WarpFm:    return frac (p + amt * 1.5 * fm);
            default:        return p;
        }
    }
}

//==============================================================================
struct FrameCursor
{
    const float* a = nullptr;
    const float* b = nullptr;
    float mix = 0;
    int size = 256;

    void set (const Wavetable& wt, int level, float pos)
    {
        const float fp = juce::jlimit (0.0f, 1.0f, pos) * (wtFrames - 1);
        const int f0 = juce::jmin (wtFrames - 2, (int) fp);
        mix = fp - (float) f0;
        a = wt.frame (f0, level);
        b = wt.frame (f0 + 1, level);
        size = wtLevelSize (level);
    }

    inline float read (double phase) const
    {
        const float p = (float) phase * (float) size;
        const int i = juce::jlimit (0, size - 1, (int) p);
        const float f = p - (float) i;
        const float s0 = a[i] + f * (a[i + 1] - a[i]);
        const float s1 = b[i] + f * (b[i + 1] - b[i]);
        return s0 + mix * (s1 - s0);
    }
};

//==============================================================================
class Voice
{
public:
    int note = -1;
    float velocity = 0;
    juce::uint64 age = 0;
    int trigger = -1; // the key that started this voice (chord voices share their root's key)
    bool held = false;

    bool isActive() const { return ampEnv.active(); }
    float ampLevel() const { return ampEnv.value; }

    // Modulated values of the last rendered sub-block, for the UI.
    float shownPos[2] {}, shownLfo[2] {}, shownCutoff = 0;
    double shownLfoPhase[2] {};
    float lastSrc[NumSrc] {}; // per-voice mod sources of the last sub-block (drives global FX destinations)

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        rng.s = 0x12345u + (juce::uint32) (reinterpret_cast<juce::pointer_sized_uint> (this) & 0xffff) * 2654435761u;
        reset();
    }

    void reset()
    {
        ampEnv.kill();
        modEnv.kill();
        for (auto& f : svf) for (auto& s : f) s.reset();
        note = -1;
    }

    // glideFrom < 0 = start straight on the note.
    // delaySamples > 0 holds the note back (chord strum).
    void start (int midiNote, float vel, float glideFrom, bool retrigger, const SynthSettings& s, const GlobalMod& g, juce::uint64 stamp,
                int delaySamples = 0)
    {
        const bool wasActive = isActive();
        startDelay = delaySamples;
        note = midiNote;
        velocity = vel;
        age = stamp;
        held = true;
        targetPitch = (float) midiNote;
        currentPitch = glideFrom >= 0 ? glideFrom : (float) midiNote;

        if (! retrigger && wasActive)
            return;

        pitchEnv = 1.0f;
        noteRandom = rng.next();
        noteAge = 0;
        if (s.phaseRetrig || ! wasActive)
        {
            for (int o = 0; o < 2; ++o)
                for (int u = 0; u < MaxUnison; ++u)
                    phase[o][u] = s.phaseRetrig ? (u == 0 ? 0.0 : dsp::frac (u * 0.3819660113)) : dsp::frac (0.5 + 0.5 * rng.next());
            subPhase = 0;
            for (auto& o : fmAcc) for (auto& a : o) a = 0;
        }
        if (! wasActive)
        {
            fmPrev[0] = fmPrev[1] = 0;
            for (auto& f : svf) for (auto& sv : f) sv.reset();
            for (auto& c : comb) std::fill (std::begin (c), std::end (c), 0.0f);
            driftValue = 0.3f * rng.next();
        }
        for (int l = 0; l < 2; ++l)
        {
            if (s.lfo[(size_t) l].retrig) lfoPhase[l] = 0;
            else lfoPhase[l] = g.lfoPhase[(size_t) l];
            lfoHeld[l] = rng.next();
            lfoPrevHeld[l] = lfoHeld[l];
        }
        ampEnv.noteOn();
        modEnv.noteOn();
    }

    // Glide to another note without retriggering (legato / note stack fallbacks).
    void slideTo (int midiNote)
    {
        note = midiNote;
        targetPitch = (float) midiNote;
        held = true;
    }

    void stop() { held = false; ampEnv.noteOff(); modEnv.noteOff(); }
    float pitchNow() const { return currentPitch; }

    void render (float* outL, float* outR, int numSamples, const SynthSettings& s, const GlobalMod& g)
    {
        if (! isActive()) return;
        if (startDelay > 0)
        {
            const int skip = juce::jmin (startDelay, numSamples);
            startDelay -= skip;
            outL += skip; outR += skip; numSamples -= skip;
            if (numSamples <= 0) return;
        }
        ampEnv.set (s.env[0], sr);
        modEnv.set (s.env[1], sr);

        const auto& bank = WavetableBank::get();
        const double glideCoef = s.glide > 0.0005f ? std::exp (-4.6 * SubBlock / (s.glide * sr)) : 0.0;
        const float pitchEnvCoef = (float) std::exp (-4.6 * SubBlock / (juce::jmax (0.001f, s.pitchEnvDecay) * sr));

        for (int start = 0; start < numSamples; start += SubBlock)
        {
            const int n = juce::jmin (SubBlock, numSamples - start);

            // ---- control rate ----
            currentPitch = (float) (targetPitch + (currentPitch - targetPitch) * glideCoef);
            float lfoVal[2];
            noteAge += (float) n / (float) sr;
            for (int l = 0; l < 2; ++l)
            {
                const auto& ls = s.lfo[(size_t) l];
                const float fade = ls.fade > 0.001f ? juce::jmin (1.0f, noteAge / ls.fade) : 1.0f;
                lfoVal[l] = dsp::lfoShape (ls.shape, lfoPhase[l], lfoHeld[l], lfoPrevHeld[l]) * fade;
                shownLfo[l] = lfoVal[l];
                shownLfoPhase[l] = lfoPhase[l];
                lfoPhase[l] += ls.rateHz * std::exp2 (juce::jlimit (-1.0f, 1.0f, lfoRateMod[l]) * 3.0f) * n / sr;
                if (lfoPhase[l] >= 1.0)
                {
                    lfoPhase[l] = dsp::frac (lfoPhase[l]);
                    lfoPrevHeld[l] = lfoHeld[l];
                    lfoHeld[l] = rng.next();
                }
            }

            float src[NumSrc] {};
            src[SrcLfo1] = lfoVal[0];
            src[SrcLfo2] = lfoVal[1];
            src[SrcEnv2] = modEnv.value;
            src[SrcVelocity] = velocity;
            src[SrcModWheel] = g.modWheel;
            src[SrcNote] = ((float) note - 60.0f) / 24.0f;
            for (int m = 0; m < 4; ++m) src[SrcMacro1 + m] = g.macros[(size_t) m];
            src[SrcRandom] = noteRandom;

            std::copy (std::begin (src), std::end (src), std::begin (lastSrc));

            float dst[NumDest] {};
            for (const auto& slot : s.mod)
                if (slot.src != SrcNone && slot.dest > DNone && slot.dest < NumDest)
                    dst[slot.dest] += src[slot.src] * slot.amount;
            lfoRateMod[0] = dst[DLfo1Rate];
            lfoRateMod[1] = dst[DLfo2Rate];

            // Analog drift: a slow random walk of the whole voice's pitch.
            if (s.drift > 0.001f)
            {
                driftTimer -= n;
                if (driftTimer <= 0) { driftTarget = rng.next(); driftTimer = (int) (sr * (0.25 + 0.2 * (1.0 + rng.next()))); }
                driftValue += (driftTarget - driftValue) * (float) n * 3.0f / (float) sr;
            }

            const float basePitch = currentPitch + g.bendSemis + s.transpose + s.pitchEnvAmt * pitchEnv + dst[DPitch] * 12.0f
                                    + s.drift * 0.35f * driftValue;
            pitchEnv *= pitchEnvCoef;

            struct OscRun
            {
                bool on = false;
                FrameCursor cur;
                int n = 1, warp = 0;
                float warpAmt = 0, crush = 256, level = 0;
                double fmScale = 0; // true FM: cycles of phase added per sample per unit of modulator
                double inc[MaxUnison] {};
                float gl[MaxUnison] {}, gr[MaxUnison] {};
            } run[2];

            for (int o = 0; o < 2; ++o)
            {
                const auto& os = s.osc[(size_t) o];
                auto& r = run[o];
                const float level = juce::jlimit (0.0f, 1.5f, os.level + dst[o == 0 ? DALevel : DBLevel]);
                r.on = os.on && level > 0.0001f;
                if (! r.on) continue;
                r.level = level;
                const float pos = juce::jlimit (0.0f, 1.0f, os.pos + dst[o == 0 ? DAPos : DBPos]);
                shownPos[o] = pos;
                r.warp = os.warp;
                r.warpAmt = juce::jlimit (0.0f, 1.0f, os.warpAmt + dst[o == 0 ? DAWarp : DBWarp]);
                r.crush = std::exp2 (8.0f - r.warpAmt * 6.0f);
                r.n = juce::jlimit (1, MaxUnison, os.unison);
                const float det = juce::jlimit (0.0f, 1.0f, os.detune + dst[o == 0 ? DADetune : DBDetune]);
                const double freq = 440.0 * std::exp2 ((basePitch + os.pitch - 69.0) / 12.0);
                const float spreadCents = det * 25.0f + det * det * 35.0f;
                const float maxDetRatio = std::exp2 (spreadCents / 1200.0f);
                const int level2 = WavetableBank::levelFor (freq * maxDetRatio * (r.warp == WarpSync ? 1.0 + r.warpAmt * 7.0 : 1.0), sr);
                r.cur.set (bank.table (os.table), level2, pos);

                float norm = 0;
                for (int u = 0; u < r.n; ++u)
                {
                    const float spread = r.n == 1 ? 0.0f : (2.0f * u / (r.n - 1) - 1.0f);
                    r.inc[u] = freq * std::exp2 (spread * spreadCents / 1200.0f) / sr;
                    const float gain = 1.0f - std::abs (spread) * (1.0f - os.blend);
                    norm += gain * gain;
                    const float p = juce::jlimit (-1.0f, 1.0f, spread * os.width + os.pan + dst[DPan]);
                    const float ang = (p + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                    r.gl[u] = gain * std::cos (ang) * 1.41421356f;
                    r.gr[u] = gain * std::sin (ang) * 1.41421356f;
                }
                const float k = level / std::sqrt (juce::jmax (norm, 1.0e-4f));
                for (int u = 0; u < r.n; ++u) { r.gl[u] *= k; r.gr[u] *= k; }
            }

            // True FM like a DX: the other oscillator bends this one's frequency. Scaled by the modulator's
            // frequency so WARP maps to a constant FM index (100% = index ~9.4) across the keyboard.
            for (int o = 0; o < 2; ++o)
                if (run[o].on && run[o].warp == WarpFm)
                    run[o].fmScale = run[o].warpAmt * 1.5 * juce::MathConstants<double>::twoPi * (run[1 - o].on ? run[1 - o].inc[0] : 0.0);

            const double subInc = s.subOn ? 440.0 * std::exp2 ((basePitch + 12.0f * s.subOct - 69.0) / 12.0) / sr : 0.0;
            const float subLevel = s.subOn ? juce::jlimit (0.0f, 1.5f, s.subLevel + dst[DSub]) : 0.0f;
            const float noiseLevel = juce::jlimit (0.0f, 1.0f, s.noiseLevel + dst[DNoise]);
            const float noiseCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * (200.0f * std::pow (100.0f, s.noiseTone)) / (float) sr);

            // Filter coefficients for this sub-block.
            const float cutoff = juce::jlimit (20.0f, (float) sr * 0.45f,
                s.cutoff * std::exp2 (s.filterEnv * 6.0f * modEnv.value + s.keytrack * ((float) note - 60.0f) / 12.0f + dst[DCutoff] * 5.0f));
            shownCutoff = cutoff;
            const float res = juce::jlimit (0.0f, 1.0f, s.res + dst[DRes]);
            const float gF = std::tan (juce::MathConstants<float>::pi * cutoff / (float) sr);
            const bool twoStage = s.filterType == FLp24 || s.filterType == FDirty;
            const float kRes = 2.0f - 1.96f * res;
            for (int c = 0; c < 2; ++c)
            {
                svf[c][0].set (gF, twoStage ? 1.414f : kRes);
                svf[c][1].set (gF, kRes);
            }
            if (s.filterType == FFormant)
            {
                // Cutoff sweeps the vowel a > e > i > o > u; resonance narrows the formants.
                static const float f1[] = { 730, 530, 270, 570, 300 }, f2[] = { 1090, 1840, 2290, 840, 870 };
                const float v = juce::jlimit (0.0f, 0.999f, std::log2 (cutoff / 20.0f) / std::log2 (1000.0f)) * 4.0f;
                const int vi = (int) v;
                const float vf = v - (float) vi;
                const float F1 = f1[vi] + (f1[vi + 1] - f1[vi]) * vf, F2 = f2[vi] + (f2[vi + 1] - f2[vi]) * vf;
                const float kF = 1.2f - 1.0f * std::sqrt (res);
                for (int c = 0; c < 2; ++c)
                {
                    svf[c][0].set (std::tan (juce::MathConstants<float>::pi * F1 / (float) sr), kF);
                    svf[c][1].set (std::tan (juce::MathConstants<float>::pi * F2 / (float) sr), kF);
                }
            }
            const float combDelay = juce::jlimit (2.0f, (float) combSize - 2.0f, (float) sr / cutoff);
            const float combFb = res * 0.995f;
            const float drive = juce::jlimit (0.0f, 1.0f, s.filterDrive + dst[DDrive]);
            const float driveGain = 1.0f + drive * drive * 15.0f + (s.filterType == FDirty ? 2.0f : 0.0f);
            const float driveNorm = 1.0f / std::sqrt (driveGain);
            const float ampMod = juce::jlimit (0.0f, 2.0f, 1.0f + dst[DAmp]);
            const float velGain = 1.0f - s.velSens + s.velSens * velocity;

            // ---- audio rate ----
            for (int i = 0; i < n; ++i)
            {
                float fL = 0, fR = 0, dL = 0, dR = 0; // filtered bus / direct bus
                float oscOut[2] = { 0, 0 };

                for (int o = 0; o < 2; ++o)
                {
                    auto& r = run[o];
                    if (! r.on) continue;
                    const float fm = o == 0 ? fmPrev[1] : oscOut[0];
                    float sumL = 0, sumR = 0, mono = 0;
                    for (int u = 0; u < r.n; ++u)
                    {
                        double& ph = phase[o][u];
                        ph += r.inc[u];
                        if (ph >= 1.0) ph -= 1.0;
                        double wp;
                        if (r.warp == WarpFm)
                        {
                            double& acc = fmAcc[o][u];
                            acc += r.fmScale * fm;
                            acc -= std::floor (acc);
                            wp = dsp::frac (ph + acc);
                        }
                        else
                            wp = r.warp == WarpOff ? ph : dsp::warpPhase (r.warp, ph, r.warpAmt, r.crush, fm);
                        const float v = r.cur.read (wp);
                        sumL += v * r.gl[u];
                        sumR += v * r.gr[u];
                        mono += v;
                    }
                    oscOut[o] = mono / (float) r.n;
                    if (s.osc[(size_t) o].toFilter) { fL += sumL; fR += sumR; }
                    else                            { dL += sumL; dR += sumR; }
                }
                fmPrev[0] = oscOut[0];
                fmPrev[1] = oscOut[1];

                if (subLevel > 0.0f)
                {
                    subPhase += subInc;
                    if (subPhase >= 1.0) subPhase -= 1.0;
                    float v;
                    switch (s.subShape)
                    {
                        case SubTri:    v = (float) (subPhase < 0.25 ? 4.0 * subPhase : (subPhase < 0.75 ? 2.0 - 4.0 * subPhase : 4.0 * subPhase - 4.0)); break;
                        case SubSquare:
                        {
                            v = subPhase < 0.5 ? 1.0f : -1.0f;
                            v += dsp::polyBlep (subPhase, subInc);
                            v -= dsp::polyBlep (dsp::frac (subPhase + 0.5), subInc);
                            v *= 0.7f;
                            break;
                        }
                        case SubPunch:  v = std::tanh (2.2f * (float) std::sin (juce::MathConstants<double>::twoPi * subPhase)) * 1.02f; break;
                        default:        v = (float) std::sin (juce::MathConstants<double>::twoPi * subPhase); break;
                    }
                    v *= subLevel;
                    if (s.subToFilter) { fL += v; fR += v; } else { dL += v; dR += v; }
                }

                if (noiseLevel > 0.0f)
                {
                    noiseState[0] += noiseCoef * (rng.next() - noiseState[0]);
                    noiseState[1] += noiseCoef * (rng.next() - noiseState[1]);
                    const float nl = noiseState[0] * noiseLevel * 1.6f, nr = noiseState[1] * noiseLevel * 1.6f;
                    if (s.noiseToFilter) { fL += nl; fR += nr; } else { dL += nl; dR += nr; }
                }

                if (s.filterOn)
                {
                    float* ch[2] = { &fL, &fR };
                    for (int c = 0; c < 2; ++c)
                    {
                        float x = *ch[c];
                        const float dry = x;
                        if (drive > 0.001f || s.filterType == FDirty)
                            x = std::tanh (x * driveGain) * driveNorm * 1.6f;
                        float lo, bp, hp;
                        svf[c][0].tick (x, lo, bp, hp);
                        float y;
                        switch (s.filterType)
                        {
                            case FLp12:  y = lo; break;
                            case FHp12:  y = hp; break;
                            case FBp:    y = bp * (0.5f + kRes * 0.5f); break;
                            case FNotch: y = lo + hp; break;
                            case FFormant:
                            {
                                float lo2, bp2, hp2;
                                svf[c][1].tick (x, lo2, bp2, hp2);
                                y = (bp * 1.4f + bp2 * 1.0f) * 1.3f;
                                break;
                            }
                            case FComb:
                            {
                                float r = (float) combPos - combDelay;
                                if (r < 0) r += (float) combSize;
                                const int i0 = (int) r, i1 = (i0 + 1) % combSize;
                                const float fr = r - (float) i0;
                                const float d = comb[c][i0] + fr * (comb[c][i1] - comb[c][i0]);
                                combDamp[c] = d + 0.3f * (combDamp[c] - d);   // gentle damping, like a real string
                                const float w = x + combFb * combDamp[c];
                                comb[c][combPos] = w;
                                y = w * (1.0f - combFb * 0.35f);
                                break;
                            }
                            case FDirty:
                            {
                                float lo2, bp2, hp2;
                                svf[c][1].tick (std::tanh (lo * 1.5f), lo2, bp2, hp2);
                                y = lo2;
                                break;
                            }
                            default:
                            {
                                float lo2, bp2, hp2;
                                svf[c][1].tick (lo, lo2, bp2, hp2);
                                y = lo2;
                                break;
                            }
                        }
                        *ch[c] = dry + (y - dry) * s.filterMix;
                    }
                    if (s.filterType == FComb) combPos = (combPos + 1) % combSize;
                }

                const float amp = ampEnv.tick() * velGain * ampMod;
                modEnv.tick();
                outL[start + i] += (fL + dL) * amp;
                outR[start + i] += (fR + dR) * amp;
            }

            if (! ampEnv.active())
            {
                note = -1;
                break;
            }
        }
    }

private:
    double sr = 44100.0;
    double phase[2][MaxUnison] {};
    double subPhase = 0;
    double fmAcc[2][MaxUnison] {};
    float fmPrev[2] {};
    float noiseState[2] {};
    float targetPitch = 60, currentPitch = 60, pitchEnv = 0, noteRandom = 0, noteAge = 0;
    float lfoRateMod[2] {};
    float driftValue = 0, driftTarget = 0;
    int driftTimer = 0, startDelay = 0;
    static constexpr int combSize = 4096;
    float comb[2][combSize] {};
    float combDamp[2] {};
    int combPos = 0;
    double lfoPhase[2] {};
    float lfoHeld[2] {}, lfoPrevHeld[2] {};
    dsp::Env ampEnv, modEnv;
    dsp::SVF svf[2][2];
    dsp::Rng rng;
};

} // namespace ab
