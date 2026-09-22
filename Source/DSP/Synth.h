#pragma once

#include <unordered_map>

#include "Wavetables.h"
#include "Sampler.h"
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

enum LfoShape { LSine, LTri, LSaw, LRamp, LSquare, LSnH, LSmooth, LDrawn, NumLfoShapes };
inline juce::StringArray lfoShapeNames() { return { "Sine", "Triangle", "Saw Down", "Ramp Up", "Square", "Sample & Hold", "Smooth Random", "Drawn" }; }

constexpr int NumLfo = 4;           // LFO 1 and 2 on the modulation page, 3 and 4 as widgets
constexpr int LfoTableSize = 256;   // a drawn LFO shape, sampled once per cycle (plus a wrap-around point)

enum VoiceMode { ModePoly, ModeMono, ModeLegato, NumModes };
inline juce::StringArray voiceModeNames() { return { "Poly", "Mono", "Legato" }; }

enum ModSrc { SrcNone, SrcLfo1, SrcLfo2, SrcEnv2, SrcVelocity, SrcModWheel, SrcNote, SrcMacro1, SrcMacro2, SrcMacro3, SrcMacro4, SrcRandom,
              SrcLfo3, SrcLfo4, SrcFollow, SrcPressure, SrcSlide, NumSrc }; // append only
inline juce::StringArray modSrcNames()
{
    return { "-", "LFO 1", "LFO 2", "Mod Env", "Velocity", "Mod Wheel", "Note", "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Random", "LFO 3", "LFO 4", "Follower", "Pressure", "Slide" };
}

// Append only (saved sessions store indices). Everything from DDistFx on is global: the processor applies it to the effects.
enum ModDest { DNone, DAPos, DBPos, DAWarp, DBWarp, DALevel, DBLevel, DPitch, DCutoff, DRes, DDrive, DSub, DNoise, DADetune, DBDetune, DPan, DAmp,
               DLfo1Rate, DLfo2Rate, DDistFx, DOttFx, DChorusFx, DDelayFx, DReverbFx, DShimmerFx, DSmpLevel,
               // Every effect control (appended): applied to the effects once per block.
               DFxFltFreq, DFxFltRes, DFxFltSweep, DDelayFb, DDelayTone, DReverbSize, DFlangRate, DFlangDepth, DFlangFb, DFlangMix,
               DTapeWow, DTapeNoise, DTapeSat, DGateDepth, DGateShape, DPanDepth, DShiftMix, DEqLow, DEqHigh, DWidth, DChorusRate,
               DDistMix, DLowLevel, DLowDuck, DLowDrive, DEqMid, DEqMidFreq,
               // Oscillators C to H (appended).
               DCPos, DDPos, DEPos, DFPos, DGPos, DHPos, DCLevel, DDLevel, DELevel, DFLevel, DGLevel, DHLevel,
               DLfo3Rate, DLfo4Rate, DRackMix, DCrushMix, DCrushBits, NumDest };
constexpr int FirstGlobalDest = DDistFx;
constexpr int FirstFxParamDest = DFxFltFreq;
// The effects are applied once per block from the newest voice's sources; the sampler, oscillators C-H
// and the LFO rates belong to each voice. New global destinations are listed here as they're appended.
inline bool isGlobalDest (int d)
{
    if (d == DRackMix || d == DCrushMix || d == DCrushBits) return true;
    return (d >= FirstGlobalDest && d < DSmpLevel) || (d > DSmpLevel && d < DCPos);
}
inline juce::StringArray modDestNames()
{
    return { "-", "A Position", "B Position", "A Warp", "B Warp", "A Level", "B Level", "Pitch", "Cutoff", "Resonance",
             "Filter Drive", "Sub Level", "Noise Level", "A Detune", "B Detune", "Pan", "Volume",
             "LFO 1 Rate", "LFO 2 Rate", "FX Distortion", "FX OTT", "FX Chorus", "FX Delay", "FX Reverb", "FX Shimmer",
             "Sample Level",
             "FX Filter Freq", "FX Filter Res", "FX Filter Sweep", "Delay Feedback", "Delay Tone", "Reverb Size", "Flanger Rate",
             "Flanger Depth", "Flanger Feedback", "Flanger Mix", "Tape Wobble", "Tape Noise", "Tape Saturation", "Gate Depth",
             "Gate Shape", "Auto Pan", "Pitch Shift Mix", "EQ Low", "EQ High", "Stereo Width", "Chorus Rate", "Distortion Mix",
             "Low End Level", "Low End Duck", "Low End Warmth", "EQ Mid", "EQ Mid Freq",
             "C Position", "D Position", "E Position", "F Position", "G Position", "H Position",
             "C Level", "D Level", "E Level", "F Level", "G Level", "H Level", "LFO 3 Rate", "LFO 4 Rate", "Rack Mix", "Crush Mix", "Crush Bits" };
}

// The knob each destination corresponds to, so a modulation source can be dropped straight onto a control
// and so every knob can draw the depth reaching it. Destinations with no single knob map to an empty string.
inline juce::String modDestParam (int dest)
{
    static const char* ids[] = { "", "aPos", "bPos", "aWarpAmt", "bWarpAmt", "aLevel", "bLevel", "", "cutoff", "res",
                                 "fltDrive", "subLevel", "noiseLevel", "aDetune", "bDetune", "", "",
                                 "lfo1Rate", "lfo2Rate", "distDrive", "ott", "chorusMix", "dlyMix", "verbMix", "verbShimmer",
                                 "smpLevel",
                                 "fxFltFreq", "fxFltRes", "fxFltDepth", "dlyFb", "dlyTone", "verbSize", "flangRate", "flangDepth", "flangFb",
                                 "flangMix", "tapeWow", "tapeNoise", "tapeSat", "gateDepth", "gateShape", "panDepth", "shiftMix", "eqLow",
                                 "eqHigh", "width", "chorusRate", "distMix", "lowLevel", "lowDuck", "lowDrive",
                                 "eqMidGain", "eqMidFreq",
                                 "cPos", "dPos", "ePos", "fPos", "gPos", "hPos", "cLevel", "dLevel", "eLevel", "fLevel", "gLevel", "hLevel",
                                 "lfo3Rate", "lfo4Rate", "fxMix", "crushMix", "crushBits" };
    return juce::isPositiveAndBelow (dest, (int) (sizeof (ids) / sizeof (ids[0]))) ? juce::String (ids[dest]) : juce::String();
}

inline int modDestForParam (const juce::String& paramId)
{
    // Built once: called for every knob on every UI tick.
    static const std::unordered_map<juce::String, int> lookup = []
    {
        std::unordered_map<juce::String, int> m;
        for (int d = 1; d < NumDest; ++d)
            if (modDestParam (d).isNotEmpty()) m[modDestParam (d)] = d;
        return m;
    }();
    const auto it = lookup.find (paramId);
    return it != lookup.end() ? it->second : 0;
}

constexpr int NumModSlots = 8;
constexpr int MaxUnison = 7;
constexpr int NumOsc = 8; // A and B, plus C to H that a sound can switch on
inline juce::String oscPrefix (int o) { return juce::String::charToString ((juce::juce_wchar) ('a' + o)); }
constexpr int MaxVoices = 20;   // 16 playable + spares so a stolen/retriggered voice can fade out instead of clicking
constexpr int PolyLimit = 16;
constexpr int SubBlock = 16;

//==============================================================================
struct OscSettings
{
    bool on = true, toFilter = true;
    const Wavetable* custom = nullptr; // an imported table replacing the factory one (owned by the processor)
    int table = 0, warp = WarpOff, unison = 1;
    float pos = 0, warpAmt = 0, detune = 0.2f, blend = 0.5f, level = 0.8f, pan = 0, pitch = 0, width = 1.0f; // pitch in semitones
};

struct EnvSettings { float a = 0.001f, d = 0.3f, s = 1.0f, r = 0.2f; };

struct LfoSettings
{
    int shape = LSine;
    float rateHz = 1.0f, fade = 0;
    bool retrig = true;
    bool once = false;              // one cycle per note, then it holds: a shape you draw becomes an envelope
    float drawnEnd = 1.0f;          // where the drawn shape's last point is, so "once" holds on that value
    const float* table = nullptr;   // the drawn shape
};

// How a modulation is shaped on its way to the knob, and how quickly it's allowed to move.
enum ModShape { ShapeLinear, ShapeExp, ShapeLog, ShapeS, ShapeSteps4, ShapeSteps8, ShapeSteps16, NumModShapes };
inline juce::StringArray modShapeNames() { return { "Linear", "Exponential", "Logarithmic", "S-Curve", "4 Steps", "8 Steps", "16 Steps" }; }

inline float shapeMod (int shape, float v)
{
    const float sign = v < 0.0f ? -1.0f : 1.0f, a = std::abs (juce::jlimit (-1.0f, 1.0f, v));
    switch (shape)
    {
        case ShapeExp:  return sign * a * a;
        case ShapeLog:  return sign * std::sqrt (a);
        case ShapeS:    return sign * a * a * (3.0f - 2.0f * a);
        case ShapeSteps4:  return sign * std::round (a * 4.0f) / 4.0f;
        case ShapeSteps8:  return sign * std::round (a * 8.0f) / 8.0f;
        case ShapeSteps16: return sign * std::round (a * 16.0f) / 16.0f;
        default: return v;
    }
}

struct ModSlot { int src = SrcNone, dest = DNone; float amount = 0; int shape = ShapeLinear; float smooth = 0; };

struct SynthSettings
{
    std::array<OscSettings, NumOsc> osc;
    bool subOn = false, subToFilter = false;
    int subShape = SubSine, subOct = -1;
    float subLevel = 0.6f;
    float noiseLevel = 0, noiseTone = 0.7f;
    int noiseType = 0;
    bool noiseToFilter = true;

    // Audio-rate cross modulation between the oscillators (independent of the FM warp).
    float xFmAB = 0, xFmBA = 0, xRing = 0, xAm = 0, xFltFm = 0;

    float pitchEnvAmt = 0, pitchEnvDecay = 0.05f;
    float glide = 0;
    int mode = ModePoly;
    bool phaseRetrig = true;

    bool filterOn = true;
    int filterType = FLp24;
    float cutoff = 18000, res = 0.1f, filterDrive = 0, filterEnv = 0, keytrack = 0, filterMix = 1.0f;

    std::array<EnvSettings, 2> env;
    std::array<LfoSettings, NumLfo> lfo;
    std::array<ModSlot, NumModSlots> mod;
    float velSens = 0.5f;
    float transpose = 0, drift = 0; // semitones (incl. fine tune), 0..1
    SamplerSettings smp;
};

struct GlobalMod
{
    float modWheel = 0, bendSemis = 0;
    std::array<float, 4> macros {};
    std::array<double, NumLfo> lfoPhase {}; // free/host-locked phase at block start, used by non-retriggered LFOs
    float follower = 0;                     // how loud the synth itself is right now (0 to 1), for self-ducking and the like
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
        float value = 0, attInc = 1, decCoef = 0, relCoef = 0, sus = 1, fastCoef = 0;
        bool fast = false; // quick de-click fade, overrides the release time

        void set (const EnvSettings& e, double sr)
        {
            attInc = (float) (1.0 / juce::jmax (1.0, juce::jmax (0.0015, (double) e.a) * sr)); // 1.5 ms floor: an instant step onto a random phase clicks
            decCoef = (float) std::exp (-6.9078 / juce::jmax (1.0, e.d * sr));
            relCoef = (float) std::exp (-6.9078 / juce::jmax (1.0, e.r * sr));
            sus = e.s;
        }
        void noteOn() { stage = Attack; fast = false; }
        void fadeOut (double sr) { if (stage != Idle) { stage = Release; fast = true; fastCoef = (float) std::exp (-6.9078 / (0.004 * sr)); } }
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
                    value *= fast ? fastCoef : relCoef;
                    if (value < 1.0e-5f) { value = 0; stage = Idle; }
                    break;
                case Idle: break;
            }
            return value;
        }
    };

    // Noise flavours. Appended only: saved sessions store the index.
    enum NoiseType { NWhite, NPink, NBrown, NBlue, NCrackle, NHiss, NDigital, NWind, NumNoiseTypes };
    inline juce::StringArray noiseNames()
    {
        return { "White", "Pink", "Brown", "Blue", "Vinyl Crackle", "Tape Hiss", "Digital", "Wind" };
    }

    struct Rng
    {
        juce::uint32 s = 0x9e3779b9u;
        inline float next() // -1..1
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return (float) s * (2.0f / 4294967295.0f) - 1.0f;
        }
    };

    // One noise voice per channel: white through a flavour-specific colouring stage.
    struct NoiseGen
    {
        Rng rng;
        float pink[3] {}, brown = 0, last = 0, held = 0, bp1 = 0, bp2 = 0, crackleEnv = 0, windPhase = 0;
        int holdCount = 0;

        void reset (juce::uint32 seed)
        {
            *this = NoiseGen();
            rng.s = seed | 1u;
        }

        inline float tick (int type, double sr)
        {
            const float w = rng.next();
            switch (type)
            {
                case NPink: // Paul Kellet's economy pink filter
                    pink[0] = 0.99765f * pink[0] + w * 0.0990460f;
                    pink[1] = 0.96300f * pink[1] + w * 0.2965164f;
                    pink[2] = 0.57000f * pink[2] + w * 1.0526913f;
                    return (pink[0] + pink[1] + pink[2] + w * 0.1848f) * 0.55f;
                case NBrown: // leaky integrator: -6 dB/octave rumble
                    brown = juce::jlimit (-1.0f, 1.0f, brown + w * 0.035f) * 0.999f;
                    return brown * 3.2f;
                case NBlue: // differentiated white: airy top end
                {
                    const float out = (w - last) * 0.55f;
                    last = w;
                    return out;
                }
                case NCrackle: // vinyl: sparse decaying pops over a quiet bed
                {
                    if (rng.next() > 0.9985f) crackleEnv = 1.0f;
                    crackleEnv *= (float) std::exp (-1200.0 / sr);
                    return w * (0.06f + crackleEnv * 1.6f);
                }
                case NHiss: // tape: band-limited hiss that breathes
                {
                    bp1 += 0.22f * (w - bp1);
                    bp2 += 0.012f * (bp1 - bp2);
                    windPhase += (float) (0.7 / sr);
                    if (windPhase >= 1.0f) windPhase -= 1.0f;
                    const float breathe = 0.85f + 0.15f * std::sin (juce::MathConstants<float>::twoPi * windPhase);
                    return (bp1 - bp2) * 2.6f * breathe;
                }
                case NDigital: // sample and hold: gritty, aliased
                {
                    if (--holdCount <= 0) { held = w; holdCount = 6; }
                    return held;
                }
                case NWind: // noise through a slowly sweeping band-pass
                {
                    windPhase += (float) (0.13 / sr);
                    if (windPhase >= 1.0f) windPhase -= 1.0f;
                    const float c = 0.03f + 0.12f * (0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * windPhase));
                    bp1 += c * (w - bp1);
                    bp2 += c * 0.25f * (bp1 - bp2);
                    return (bp1 - bp2) * 4.0f;
                }
                default: return w;
            }
        }
    };

    inline float lfoShape (int shape, double p, float held, float prevHeld, const float* table = nullptr)
    {
        switch (shape)
        {
            case LDrawn:
            {
                if (table == nullptr) return 0.0f;
                const double x = juce::jlimit (0.0, 1.0, p) * LfoTableSize;
                const int i = juce::jmin (LfoTableSize - 1, (int) x);
                const float t = (float) (x - i);
                return table[i] + (table[i + 1] - table[i]) * t;
            }
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
    // Two neighbouring frames (morph) at two neighbouring band limits (crossfaded as pitch moves, so a glide
    // never jumps in brightness), read with 4-point Hermite interpolation.
    const float* a = nullptr;
    const float* b = nullptr;
    const float* a2 = nullptr;
    const float* b2 = nullptr;
    float mix = 0, levelMix = 0;
    int size = 256, mask = 255, size2 = 256, mask2 = 255;

    void set (const Wavetable& wt, double exactLevel, float pos)
    {
        const float fp = juce::jlimit (0.0f, 1.0f, pos) * (wtFrames - 1);
        const int f0 = juce::jmin (wtFrames - 2, (int) fp);
        mix = fp - (float) f0;
        const int lo = juce::jlimit (0, wtLevels - 1, (int) std::ceil (exactLevel));
        const int hi = juce::jmin (wtLevels - 1, lo + 1);
        // Only the last quarter-octave before a switch reads both levels; elsewhere one level is enough.
        const float t = hi == lo ? 0.0f : juce::jlimit (0.0f, 1.0f, (float) (exactLevel - (double) lo + 1.0));
        levelMix = juce::jlimit (0.0f, 1.0f, (t - 0.75f) * 4.0f);
        a = wt.frame (f0, lo);  b = wt.frame (f0 + 1, lo);
        a2 = wt.frame (f0, hi); b2 = wt.frame (f0 + 1, hi);
        size = wtLevelSize (lo);  mask = size - 1;
        size2 = wtLevelSize (hi); mask2 = size2 - 1;
    }

    static inline float hermite (const float* d, int i, float f, int m)
    {
        const float xm1 = d[(i - 1) & m], x0 = d[i & m], x1 = d[(i + 1) & m], x2 = d[(i + 2) & m];
        const float c = (x1 - xm1) * 0.5f;
        const float v = x0 - x1;
        const float w = c + v;
        const float aa = w + v + (x2 - x0) * 0.5f;
        const float bb = w + aa;
        return ((aa * f - bb) * f + c) * f + x0;
    }

    // Hermite is linear in the samples, so blend the two morph frames tap by tap and interpolate once.
    inline float readLevel (const float* fa, const float* fb, int sz, int m, double phase) const
    {
        const float p = (float) phase * (float) sz;
        const int i = juce::jlimit (0, sz - 1, (int) p);
        const float f = p - (float) i;
        const int im1 = (i - 1) & m, i1 = (i + 1) & m, i2 = (i + 2) & m;
        const float xm1 = fa[im1] + mix * (fb[im1] - fa[im1]);
        const float x0  = fa[i]   + mix * (fb[i]   - fa[i]);
        const float x1  = fa[i1]  + mix * (fb[i1]  - fa[i1]);
        const float x2  = fa[i2]  + mix * (fb[i2]  - fa[i2]);
        const float c = (x1 - xm1) * 0.5f;
        const float v = x0 - x1;
        const float w = c + v;
        const float aa = w + v + (x2 - x0) * 0.5f;
        const float bb = w + aa;
        return ((aa * f - bb) * f + c) * f + x0;
    }

    inline float read (double phase) const
    {
        const float v = readLevel (a, b, size, mask, phase);
        if (levelMix < 0.002f) return v;
        return v + levelMix * (readLevel (a2, b2, size2, mask2, phase) - v);
    }
};

//==============================================================================
class Voice
{
public:
    int note = -1;
    int channel = 1;              // MPE: the channel this note came in on, which carries its expression
    float noteBend = 0;           // semitones, from that channel's pitch bend
    float pressure = 0, slide = 0; // how hard it's pressed and where along the key, 0..1 and -1..1
    float velocity = 0;
    juce::uint64 age = 0;
    int trigger = -1; // the key that started this voice (chord voices share their root's key)
    bool held = false;

    bool isActive() const { return ampEnv.active(); }
    float ampLevel() const { return ampEnv.value; }

    // Modulated values of the last rendered sub-block, for the UI.
    float shownPos[NumOsc] {}, shownLfo[NumLfo] {}, shownCutoff = 0, shownModEnv = 0, shownVelocity = 0;
    float shownSample = -1; // sampler playhead, 0..1 of the sample (-1 when it isn't playing)
    double shownLfoPhase[NumLfo] {};
    float lastSrc[NumSrc] {}; // per-voice mod sources of the last sub-block (drives global FX destinations)
    float slotValue[NumModSlots] {}; // each slot's source after its shape and slew (the effects use these too)

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
        fading = false;
        fadeLeft = 0;
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
        noise[0].reset ((juce::uint32) (noteRandom * 1.0e6f) + 0x2545f491u);
        noise[1].reset ((juce::uint32) (noteRandom * 1.0e6f) + 0x9e3779b1u);
        noteAge = 0;
        if (s.phaseRetrig || ! wasActive)
        {
            for (int o = 0; o < NumOsc; ++o)
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
        for (int l = 0; l < NumLfo; ++l)
        {
            if (s.lfo[(size_t) l].retrig) lfoPhase[l] = 0;
            else lfoPhase[l] = g.lfoPhase[(size_t) l];
            lfoHeld[l] = rng.next();
            lfoPrevHeld[l] = lfoHeld[l];
        }
        ampEnv.noteOn();
        modEnv.noteOn();
        smpPlay.start (s.smp);
        smpEnv.noteOn();
    }

    // Glide to another note without retriggering (legato / note stack fallbacks).
    void slideTo (int midiNote)
    {
        note = midiNote;
        targetPitch = (float) midiNote;
        held = true;
    }

    void stop() { held = false; ampEnv.noteOff(); modEnv.noteOff(); smpEnv.noteOff(); }
    // Hand the note over: this voice fades out in ~4 ms while the new note starts on another voice.
    // Smooth raised-cosine fade (~12 ms): no corner in the waveform, so no click even on a deep sub.
    void fadeOut() { held = false; fading = true; trigger = -1; fadeLen = fadeLeft = juce::jmax (1, (int) (0.012 * sr)); }
    bool fading = false;
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
        smpEnv.set ({ s.smp.a, s.smp.d, s.smp.s, s.smp.r }, sr);

        const auto& bank = WavetableBank::get();
        const double glideCoef = s.glide > 0.0005f ? std::exp (-4.6 * SubBlock / (s.glide * sr)) : 0.0;
        const float pitchEnvCoef = (float) std::exp (-4.6 * SubBlock / (juce::jmax (0.001f, s.pitchEnvDecay) * sr));

        for (int start = 0; start < numSamples; start += SubBlock)
        {
            const int n = juce::jmin (SubBlock, numSamples - start);

            // ---- control rate ----
            currentPitch = (float) (targetPitch + (currentPitch - targetPitch) * glideCoef);
            float lfoVal[NumLfo];
            noteAge += (float) n / (float) sr;
            for (int l = 0; l < NumLfo; ++l)
            {
                const auto& ls = s.lfo[(size_t) l];
                const float fade = ls.fade > 0.001f ? juce::jmin (1.0f, noteAge / ls.fade) : 1.0f;
                // "Once" holds on the last point of the shape rather than running into the wrap back to the start.
                const double ph = ls.once ? juce::jmin ((double) ls.drawnEnd, lfoPhase[l]) : lfoPhase[l];
                lfoVal[l] = dsp::lfoShape (ls.shape, ph, lfoHeld[l], lfoPrevHeld[l], ls.table) * fade;
                shownLfo[l] = lfoVal[l];
                shownModEnv = modEnv.value;
                shownVelocity = velocity;
                shownLfoPhase[l] = lfoPhase[l];
                lfoPhase[l] += ls.rateHz * std::exp2 (juce::jlimit (-1.0f, 1.0f, lfoRateMod[l]) * 3.0f) * n / sr;
                if (lfoPhase[l] >= 1.0)
                {
                    // "Once" holds the end of the shape instead of starting again: one pass per note.
                    if (ls.once) lfoPhase[l] = 0.9999;
                    else
                    {
                        lfoPhase[l] = dsp::frac (lfoPhase[l]);
                        lfoPrevHeld[l] = lfoHeld[l];
                        lfoHeld[l] = rng.next();
                    }
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
            src[SrcLfo3] = lfoVal[2];
            src[SrcLfo4] = lfoVal[3];
            src[SrcFollow] = g.follower;
            src[SrcPressure] = pressure;
            src[SrcSlide] = slide;

            std::copy (std::begin (src), std::end (src), std::begin (lastSrc));

            float dst[NumDest] {};
            for (size_t m = 0; m < s.mod.size(); ++m)
            {
                const auto& slot = s.mod[m];
                if (slot.src == SrcNone || slot.dest <= DNone || slot.dest >= NumDest) { slotValue[m] = 0.0f; continue; }
                // Shape first (exponential, stepped, ...), then slew: a slot can move as gently as you like.
                float v = shapeMod (slot.shape, src[slot.src]);
                if (slot.smooth > 0.001f)
                {
                    const float secs = 0.005f + slot.smooth * slot.smooth * 1.5f;
                    const float c = std::exp (-(float) n / ((float) sr * secs));
                    v = slotSmoothed[m] = v + (slotSmoothed[m] - v) * c;
                }
                else slotSmoothed[m] = v;
                slotValue[m] = v;
                dst[slot.dest] += v * slot.amount;
            }
            lfoRateMod[0] = dst[DLfo1Rate];
            lfoRateMod[1] = dst[DLfo2Rate];
            lfoRateMod[2] = dst[DLfo3Rate];
            lfoRateMod[3] = dst[DLfo4Rate];

            // Analog drift: a slow random walk of the whole voice's pitch.
            if (s.drift > 0.001f)
            {
                driftTimer -= n;
                if (driftTimer <= 0) { driftTarget = rng.next(); driftTimer = (int) (sr * (0.25 + 0.2 * (1.0 + rng.next()))); }
                driftValue += (driftTarget - driftValue) * (float) n * 3.0f / (float) sr;
            }

            const float basePitch = currentPitch + g.bendSemis + noteBend + s.transpose + s.pitchEnvAmt * pitchEnv + dst[DPitch] * 12.0f
                                    + s.drift * 0.35f * driftValue;
            pitchEnv *= pitchEnvCoef;

            struct OscRun
            {
                bool on = false;
                FrameCursor cur;
                int n = 1, warp = 0;
                float warpAmt = 0, crush = 256, level = 0;
                double fmScale = 0; // true FM: cycles of phase added per sample per unit of modulator
                double xScale = 0;  // the same, for the cross-modulation knobs
                double inc[MaxUnison] {};
                float gl[MaxUnison] {}, gr[MaxUnison] {};
            } run[NumOsc];

            // Modulation destinations per oscillator (warp and detune exist for A and B only).
            static constexpr int posDest[NumOsc] = { DAPos, DBPos, DCPos, DDPos, DEPos, DFPos, DGPos, DHPos };
            static constexpr int levelDest[NumOsc] = { DALevel, DBLevel, DCLevel, DDLevel, DELevel, DFLevel, DGLevel, DHLevel };
            for (int o = 0; o < NumOsc; ++o)
            {
                const auto& os = s.osc[(size_t) o];
                auto& r = run[o];
                if (! os.on) { r.on = false; continue; }
                const float level = juce::jlimit (0.0f, 1.5f, os.level + dst[levelDest[o]]);
                r.on = os.on && level > 0.0001f;
                if (! r.on) continue;
                r.level = level;
                const float pos = juce::jlimit (0.0f, 1.0f, os.pos + dst[posDest[o]]);
                shownPos[o] = pos;
                r.warp = os.warp;
                r.warpAmt = juce::jlimit (0.0f, 1.0f, os.warpAmt + (o < 2 ? dst[o == 0 ? DAWarp : DBWarp] : 0.0f));
                r.crush = std::exp2 (8.0f - r.warpAmt * 6.0f);
                r.n = juce::jlimit (1, MaxUnison, os.unison);
                const float det = juce::jlimit (0.0f, 1.0f, os.detune + (o < 2 ? dst[o == 0 ? DADetune : DBDetune] : 0.0f));
                const double freq = 440.0 * std::exp2 ((basePitch + os.pitch - 69.0) / 12.0);
                const float spreadCents = det * 25.0f + det * det * 35.0f;
                const float maxDetRatio = std::exp2 (spreadCents / 1200.0f);
                const double level2 = WavetableBank::exactLevel (freq * maxDetRatio * (r.warp == WarpSync ? 1.0 + r.warpAmt * 7.0 : 1.0), sr);
                r.cur.set (os.custom != nullptr ? *os.custom : bank.table (os.table), level2, pos);

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
            // (A is modulated by B; B and the extra oscillators by A.)
            for (int o = 0; o < NumOsc; ++o)
                if (run[o].on && run[o].warp == WarpFm)
                {
                    const int m = o == 0 ? 1 : 0;
                    run[o].fmScale = run[o].warpAmt * 1.5 * juce::MathConstants<double>::twoPi * (run[m].on ? run[m].inc[0] : 0.0);
                }

            // Cross modulation: the same index scaling as the warp, but on its own knobs so a patch can use
            // a warp and FM at once, in either direction.
            const float xFm[2] = { s.xFmBA, s.xFmAB }; // [0] = B modulates A, [1] = A modulates B
            for (int o = 0; o < 2; ++o)
                run[o].xScale = run[o].on && xFm[o] > 0.0001f
                                    ? xFm[o] * 1.5 * juce::MathConstants<double>::twoPi * (run[1 - o].on ? run[1 - o].inc[0] : 0.0)
                                    : 0.0;
            // Only the oscillators that are on get visited per sample.
            int active[NumOsc], numActive = 0;
            for (int o = 0; o < NumOsc; ++o) if (run[o].on) active[numActive++] = o;
            const bool crossOn = s.xRing > 0.0001f || s.xAm > 0.0001f || s.xFltFm > 0.0001f
                                 || run[0].xScale != 0.0 || run[1].xScale != 0.0;

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

            // Sampler: pitch follows the voice (glide, bend, drop, drift) unless key tracking is off.
            const auto& sm = s.smp;
            const bool smpOn = sm.on && sm.data != nullptr && ! smpPlay.done;
            double smpInc = 0;
            float smpGL = 0, smpGR = 0;
            if (smpOn)
            {
                const float notePart = sm.track ? basePitch : basePitch - currentPitch + (float) sm.root;
                smpInc = std::exp2 ((notePart + sm.semi + sm.fine * 0.01f - (float) sm.root) / 12.0) * sm.data->rate / sr;
                const float lvl = juce::jlimit (0.0f, 1.5f, sm.level + dst[DSmpLevel]);
                const float p = juce::jlimit (-1.0f, 1.0f, sm.pan + dst[DPan]);
                const float ang = (p + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                smpGL = lvl * std::cos (ang) * 1.41421356f;
                smpGR = lvl * std::sin (ang) * 1.41421356f;
                shownSample = (float) (smpPlay.pos / juce::jmax (1, sm.data->length));
            }
            else shownSample = -1.0f;

            // ---- audio rate ----
            for (int i = 0; i < n; ++i)
            {
                float fL = 0, fR = 0, dL = 0, dR = 0; // filtered bus / direct bus
                float oscOut[NumOsc] {};

                float oscL[NumOsc] {}, oscR[NumOsc] {};
                for (int ai = 0; ai < numActive; ++ai)
                {
                    const int o = active[ai];
                    auto& r = run[o];
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
                        if (r.xScale != 0.0)
                        {
                            double& acc = xAcc[o][u];
                            acc += r.xScale * fm;
                            acc -= std::floor (acc);
                            wp = dsp::frac (wp + acc);
                        }
                        const float v = r.cur.read (wp);
                        sumL += v * r.gl[u];
                        sumR += v * r.gr[u];
                        mono += v;
                    }
                    oscOut[o] = mono / (float) r.n;
                    oscL[o] = sumL;
                    oscR[o] = sumR;
                }

                if (crossOn && s.xAm > 0.0001f)
                {
                    // Osc B drives osc A's level: 0 % leaves A alone, 100 % is full amplitude modulation.
                    const float m = 1.0f - s.xAm + s.xAm * (0.5f + 0.5f * oscOut[1]);
                    oscL[0] *= m;
                    oscR[0] *= m;
                    oscOut[0] *= m;
                }

                for (int ai = 0; ai < numActive; ++ai)
                {
                    const int o = active[ai];
                    if (s.osc[(size_t) o].toFilter) { fL += oscL[o]; fR += oscR[o]; }
                    else                            { dL += oscL[o]; dR += oscR[o]; }
                }

                if (crossOn && s.xRing > 0.0001f && run[0].on && run[1].on)
                {
                    // Ring modulation: the product of the two oscillators, added through osc A's routing.
                    const float ringL = oscL[0] * oscOut[1] * s.xRing * 1.6f;
                    const float ringR = oscR[0] * oscOut[1] * s.xRing * 1.6f;
                    if (s.osc[0].toFilter) { fL += ringL; fR += ringR; }
                    else                   { dL += ringL; dR += ringR; }
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
                    // TONE stays a low-pass tilt on top of whichever flavour is selected.
                    noiseState[0] += noiseCoef * (noise[0].tick (s.noiseType, sr) - noiseState[0]);
                    noiseState[1] += noiseCoef * (noise[1].tick (s.noiseType, sr) - noiseState[1]);
                    const float nl = noiseState[0] * noiseLevel * 1.6f, nr = noiseState[1] * noiseLevel * 1.6f;
                    if (s.noiseToFilter) { fL += nl; fR += nr; } else { dL += nl; dR += nr; }
                }

                if (smpOn)
                {
                    float sl = 0, sr2 = 0;
                    smpPlay.tick (sm, smpInc, smpGL, smpGR, sl, sr2);
                    const float e = smpEnv.tick();
                    if (sm.toFilter) { fL += sl * e; fR += sr2 * e; } else { dL += sl * e; dR += sr2 * e; }
                }

                if (crossOn && s.xFltFm > 0.0001f && s.filterOn && s.filterType != FFormant)
                {
                    // Filter FM: osc A shifts the cutoff at audio rate, which buzzes and growls.
                    const float g2 = juce::jlimit (0.0005f, 1.4f, gF * (1.0f + s.xFltFm * 3.0f * oscOut[0]));
                    for (int c = 0; c < 2; ++c)
                    {
                        svf[c][0].set (g2, twoStage ? 1.414f : kRes);
                        svf[c][1].set (g2, kRes);
                    }
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

                float amp = ampEnv.tick() * velGain * ampMod;
                if (fading)
                {
                    const float t = (float) fadeLeft / (float) fadeLen;
                    amp *= 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * t);
                    if (--fadeLeft <= 0) { ampEnv.kill(); modEnv.kill(); fading = false; }
                }
                modEnv.tick();
                outL[start + i] += (fL + dL) * amp;
                outR[start + i] += (fR + dR) * amp;
            }

            if (! ampEnv.active())
            {
                note = -1;
                fading = false;
                break;
            }
        }
    }

private:
    double sr = 44100.0;
    double phase[NumOsc][MaxUnison] {};
    double subPhase = 0;
    double fmAcc[NumOsc][MaxUnison] {}, xAcc[NumOsc][MaxUnison] {};
    float fmPrev[2] {};
    float noiseState[2] {};
    dsp::NoiseGen noise[2];
    float targetPitch = 60, currentPitch = 60, pitchEnv = 0, noteRandom = 0, noteAge = 0;
    float lfoRateMod[NumLfo] {};
    float driftValue = 0, driftTarget = 0;
    int driftTimer = 0, startDelay = 0, fadeLen = 1, fadeLeft = 0;
    static constexpr int combSize = 4096;
    float comb[2][combSize] {};
    float combDamp[2] {};
    int combPos = 0;
    float slotSmoothed[NumModSlots] {};
    double lfoPhase[NumLfo] {};
    float lfoHeld[NumLfo] {}, lfoPrevHeld[NumLfo] {};
    dsp::Env ampEnv, modEnv, smpEnv;
    dsp::SamplePlayer smpPlay;
    dsp::SVF svf[2][2];
    dsp::Rng rng;
};

} // namespace ab
