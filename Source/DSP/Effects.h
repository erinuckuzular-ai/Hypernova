#pragma once

#include <juce_dsp/juce_dsp.h>
#include "ExtraFx.h"
#include <cmath>
#include <array>
#include <vector>

// Master effects: distortion (2x oversampled), OTT-style multiband squash, chorus, synced delay,
// reverb, and a mono-bass stage that keeps the lows centred for club systems.
namespace ab
{

enum DistType { DistTube, DistHard, DistFold, Dist808, DistRectify, DistCrush, NumDistTypes };
inline juce::StringArray distNames() { return { "Tube", "Hard Clip", "Wavefold", "808 Clip", "Rectify", "Bitcrush" }; }

inline juce::StringArray delayTimeNames() { return { "1/32", "1/16", "1/16 D", "1/8 T", "1/8", "1/8 D", "1/4 T", "1/4", "1/4 D", "1/2", "1 bar" }; }
inline double delayTimeBeats (int i)
{
    static const double b[] = { 0.125, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 4.0 };
    return b[juce::jlimit (0, 10, i)];
}

// The rack: the effects that can be put in any order. Width and mono bass stay last (they're the output stage).
// Append only: saved orders store these numbers.
enum FxId { FxDist, FxTape, FxOtt, FxPitch, FxChorus, FxFlanger, FxFilter, FxGate, FxDelay, FxReverb, FxEq, NumFx };
inline juce::StringArray fxRackNames()
{
    return { "DIST", "TAPE", "OTT", "PITCH", "CHORUS", "FLANGER", "FILTER", "GATE", "DELAY", "SPACE", "EQ" };
}
using FxOrder = std::array<juce::uint8, NumFx>;
inline FxOrder defaultFxOrder() { FxOrder o {}; for (int i = 0; i < NumFx; ++i) o[(size_t) i] = (juce::uint8) i; return o; }
// "0,1,2,..." with every effect exactly once; anything else is the default order.
inline FxOrder parseFxOrder (const juce::String& text)
{
    auto parts = juce::StringArray::fromTokens (text, ",", {});
    FxOrder o {};
    std::array<bool, NumFx> seen {};
    if (parts.size() != NumFx) return defaultFxOrder();
    for (int i = 0; i < NumFx; ++i)
    {
        const int v = parts[i].getIntValue();
        if (v < 0 || v >= NumFx || seen[(size_t) v]) return defaultFxOrder();
        seen[(size_t) v] = true;
        o[(size_t) i] = (juce::uint8) v;
    }
    return o;
}
inline juce::String fxOrderText (const FxOrder& o)
{
    juce::StringArray parts;
    for (auto v : o) parts.add (juce::String ((int) v));
    return parts.joinIntoString (",");
}

struct FxSettings
{
    FxOrder order = defaultFxOrder();
    int distType = DistTube;
    float distDrive = 0, distMix = 0;
    float ott = 0;
    float chorusMix = 0, chorusRate = 0.6f;
    int delayTime = 5;
    float delayFeedback = 0.35f, delayMix = 0;
    float reverbSize = 0.6f, reverbMix = 0, shimmer = 0;
    bool monoBass = true, delayPing = true;
    float delayTone = 0.6f, width = 1.0f, eqLow = 0, eqHigh = 0;
    float bpm = 120.0f;
    // Per-effect bypass (clicking an effect's name in the UI). Default on so older sessions are unchanged.
    bool distOn = true, ottOn = true, chorusOn = true, delayOn = true, reverbOn = true, eqOn = true;

    // Movement, character and pitch (the second effects page). All default to silent/neutral.
    int chorusMode = 0;                                    // classic / ensemble / dimension
    int delayStyle = 0;                                    // digital / reverse / granular
    int reverbMode = 0;                                    // space / plate / spring / room
    float flangerMix = 0, flangerRate = 0.3f, flangerDepth = 0.5f, flangerFeedback = 0.4f;
    float tapeWobble = 0, tapeNoise = 0, tapeSat = 0;
    float gateDepth = 0, gateShape = 0.3f, panDepth = 0;
    int gateRate = 6, gatePattern = 0, panRate = 4;
    int fxFilterType = 0;
    float fxFilterFreq = 1000.0f, fxFilterRes = 0.2f, fxFilterDepth = 0;
    int fxFilterRate = 4;
    float pitchSemis = 0, pitchMix = 0;
    bool flangerOn = true, tapeOn = true, gateOn = true, fxFilterOn = true, pitchOn = true;

    // Low End: split at the crossover; only the upper band goes through the rack, the low band stays clean.
    bool lowOn = false, lowMono = true;
    float lowXover = 120.0f, lowLevelDb = 0, lowDrive = 0, lowDuck = 0, lowDuckRelease = 0.15f;
    int lowDuckRate = 0;
    double ppq = 0;
    bool playing = false;
};

// 8-line feedback delay network with input diffusion, slow modulation, damping and an optional
// octave-up shimmer fed back into the tail. Size maps to RT60 from ~0.4 s to ~16 s.
class SpaceReverb
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int maxLine = (int) (0.22 * sr) + 8;
        for (auto& l : lines) l.assign ((size_t) maxLine, 0.0f);
        for (auto& c : diffusers) for (auto& d : c) d.buf.assign ((size_t) (0.02 * sr) + 4, 0.0f);
        pre.assign ((size_t) (0.06 * sr) + 4, 0.0f);
        shimmerBuf.assign ((size_t) (0.12 * sr) + 4, 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& l : lines) std::fill (l.begin(), l.end(), 0.0f);
        for (auto& c : diffusers) for (auto& d : c) { std::fill (d.buf.begin(), d.buf.end(), 0.0f); d.pos = 0; }
        std::fill (pre.begin(), pre.end(), 0.0f);
        std::fill (shimmerBuf.begin(), shimmerBuf.end(), 0.0f);
        for (auto& d : damp) d = 0;
        linePos = prePos = shimmerPos = 0;
        shimmerPhase = 0; shimmerLp = 0; shimmerOut = 0; lfoPhase = 0;
    }

    // Processes in place, adding the wet signal scaled by mix.
    // mode: 0 space (the big FDN), 1 plate (dense and bright), 2 spring (boingy, dispersive), 3 room (short, dark).
    void process (float* L, float* R, int n, float size, float mix, float shimmer, int mode = 0)
    {
        static const float baseMs[8] = { 37.3f, 41.9f, 47.1f, 53.3f, 59.9f, 67.7f, 73.1f, 79.3f };
        static const float diffMs[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
        // Each mode is the same network with different proportions: line lengths, decay, damping and pre-delay.
        const float sizeScale[4] = { 1.0f, 0.55f, 0.32f, 0.45f };
        const float decayScale[4] = { 1.0f, 0.7f, 0.5f, 0.35f };
        const float m = (float) juce::jlimit (0, 3, mode);
        const int mi = juce::jlimit (0, 3, mode);
        const float scale = (0.7f + 0.9f * size) * sizeScale[mi];
        const float rt60 = 0.4f * std::pow (40.0f, size) * decayScale[mi];
        float len[8], gain[8];
        for (int i = 0; i < 8; ++i)
        {
            len[i] = baseMs[i] * scale * 0.001f * (float) sr;
            gain[i] = std::pow (10.0f, -3.0f * len[i] / (rt60 * (float) sr));
        }
        const float dampHz = mi == 1 ? 12000.0f - 3000.0f * size       // plate: bright
                           : mi == 2 ? 4500.0f - 1500.0f * size        // spring: dark and metallic
                           : mi == 3 ? 6000.0f - 2000.0f * size        // room
                                     : 9000.0f - 3500.0f * size;
        const float dampC = std::exp (-juce::MathConstants<float>::twoPi * dampHz / (float) sr);
        const int preDelay = (int) ((0.008f + 0.035f * size) * (mi == 0 ? 1.0f : 0.3f) * (float) sr);
        juce::ignoreUnused (m);
        const int lineSize = (int) lines[0].size();
        const int shimSize = (int) shimmerBuf.size();
        const float shimWindow = 0.1f * (float) sr;
        const float lfoInc = 0.13f / (float) sr;
        const float modDepth = 0.0012f * (float) sr;
        const float shimLpC = std::exp (-juce::MathConstants<float>::twoPi * 7000.0f / (float) sr);

        for (int i = 0; i < n; ++i)
        {
            // Pre-delay (mono) + shimmer return, then two diffusion chains for width.
            const float in = 0.5f * (L[i] + R[i]) + shimmerOut * shimmer * 0.55f;
            pre[(size_t) prePos] = in;
            int rp = prePos - preDelay; if (rp < 0) rp += (int) pre.size();
            const float delayed = pre[(size_t) rp];
            prePos = (prePos + 1) % (int) pre.size();

            float diffused[2];
            for (int c = 0; c < 2; ++c)
            {
                float x = delayed;
                for (int d = 0; d < 4; ++d)
                    x = diffusers[c][d].process (x, (int) (diffMs[d] * (c == 0 ? 1.0f : 1.13f) * 0.001f * (float) sr), 0.62f);
                diffused[c] = x;
            }

            // Read the 8 lines (slowly modulated), mix with a Hadamard matrix, write back.
            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
            float out[8];
            for (int k = 0; k < 8; ++k)
            {
                const float mod = modDepth * std::sin (juce::MathConstants<float>::twoPi * (lfoPhase + 0.125f * k));
                float r = (float) linePos - len[k] - mod - 2.0f;
                while (r < 0) r += (float) lineSize;
                const int i0 = (int) r, i1 = (i0 + 1) % lineSize;
                const float fr = r - (float) i0;
                float v = lines[(size_t) k][(size_t) i0] + fr * (lines[(size_t) k][(size_t) i1] - lines[(size_t) k][(size_t) i0]);
                damp[k] = v + dampC * (damp[k] - v);
                out[k] = damp[k] * gain[k];
            }
            float h[8];
            hadamard (out, h);
            for (int k = 0; k < 8; ++k)
                lines[(size_t) k][(size_t) linePos] = h[k] + diffused[k & 1] * 0.5f;
            linePos = (linePos + 1) % lineSize;

            const float wetL = (out[0] - out[2] + out[4] - out[6] + out[1] * 0.5f) * 0.45f;
            const float wetR = (out[1] - out[3] + out[5] - out[7] + out[0] * 0.5f) * 0.45f;

            // Shimmer: octave-up pitch shift of the tail (two crossfaded taps sweeping a 100 ms window).
            if (shimmer > 0.001f)
            {
                const float tail = 0.5f * (wetL + wetR);
                shimmerLp = tail + shimLpC * (shimmerLp - tail);
                shimmerBuf[(size_t) shimmerPos] = shimmerLp;
                shimmerPhase += 1.0f / shimWindow; // pitch ratio 2: the read head gains one sample per sample
                if (shimmerPhase >= 1.0f) shimmerPhase -= 1.0f;
                float acc = 0;
                for (int t = 0; t < 2; ++t)
                {
                    float ph = shimmerPhase + 0.5f * t;
                    if (ph >= 1.0f) ph -= 1.0f;
                    const float d = (1.0f - ph) * shimWindow + 2.0f;
                    float r = (float) shimmerPos - d;
                    while (r < 0) r += (float) shimSize;
                    const int i0 = (int) r, i1 = (i0 + 1) % shimSize;
                    const float fr = r - (float) i0;
                    const float w = std::sin (juce::MathConstants<float>::pi * ph);
                    acc += w * w * (shimmerBuf[(size_t) i0] + fr * (shimmerBuf[(size_t) i1] - shimmerBuf[(size_t) i0]));
                }
                shimmerOut = std::tanh (acc);
                shimmerPos = (shimmerPos + 1) % shimSize;
            }
            else shimmerOut = 0;

            L[i] += wetL * mix;
            R[i] += wetR * mix;
        }
    }

private:
    struct Allpass
    {
        std::vector<float> buf;
        int pos = 0;
        inline float process (float x, int delay, float g)
        {
            const int size = (int) buf.size();
            int r = pos - juce::jlimit (1, size - 1, delay);
            if (r < 0) r += size;
            const float d = buf[(size_t) r];
            const float v = x + g * d;
            buf[(size_t) pos] = v;
            pos = (pos + 1) % size;
            return d - g * v;
        }
    };

    static void hadamard (const float* in, float* out)
    {
        float a[8];
        for (int i = 0; i < 8; ++i) a[i] = in[i];
        for (int len = 1; len < 8; len <<= 1)
            for (int i = 0; i < 8; i += len << 1)
                for (int j = i; j < i + len; ++j)
                {
                    const float x = a[j], y = a[j + len];
                    a[j] = x + y;
                    a[j + len] = x - y;
                }
        for (int i = 0; i < 8; ++i) out[i] = a[i] * 0.35355339f;
    }

    double sr = 44100.0;
    std::array<std::vector<float>, 8> lines;
    std::array<std::array<Allpass, 4>, 2> diffusers;
    std::vector<float> pre, shimmerBuf;
    float damp[8] {};
    int linePos = 0, prePos = 0, shimmerPos = 0;
    float shimmerPhase = 0, shimmerLp = 0, shimmerOut = 0, lfoPhase = 0;
};

class Effects
{
public:
    void prepare (double sampleRate, int blockSize)
    {
        sr = sampleRate;
        oversampler.initProcessing ((size_t) blockSize);
        oversampler.reset();

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2 };
        chorus.prepare (spec);
        chorus.setCentreDelay (7.0f);
        chorus.setFeedback (0.0f);
        chorus.setDepth (0.35f);
        reverb.prepare (sampleRate);

        for (auto* f : { &lowSplit, &highSplit })
        {
            f->prepare (spec);
            f->reset();
        }
        lowSplit.setCutoffFrequency (120.0f);
        highSplit.setCutoffFrequency (2500.0f);
        lowSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        highSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        for (auto& e : ottEnv) e = 0;
        lowEnd.prepare (spec);
        lowEnd.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        lowEnd.setCutoffFrequency (120.0f);
        lowEnd.reset();
        lowXoverNow = 120.0f;
        lowBuf.setSize (2, blockSize);
        lowGainSmoothed = 1.0f;
        duckBeats = 0;

        const int maxDelay = (int) (sampleRate * 4.5) + 8;
        for (auto& d : delayLine) d.assign ((size_t) maxDelay, 0.0f);
        delayWrite = 0;
        delaySmoothed = -1;
        for (auto& s : delayFilt) s = 0;

        flanger.prepare (sampleRate);
        tape.prepare (sampleRate);
        gate.prepare (sampleRate);
        fxFilter.prepare (sampleRate);
        shifter.prepare (sampleRate);

        scratch.setSize (2, blockSize);
        for (auto& f : eqLowF) f.reset();
        for (auto& f : eqHighF) f.reset();
        lastEqLow = lastEqHigh = 1000.0f;
        dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0;
        sideHp[0] = sideHp[1] = 0;
        crushHold[0] = crushHold[1] = 0;
        crushCount = 0;
    }

    void reset()
    {
        oversampler.reset();
        chorus.reset();
        reverb.reset();
        for (auto& d : delayLine) std::fill (d.begin(), d.end(), 0.0f);
        flanger.reset();
        tape.reset();
        fxFilter.reset();
        shifter.reset();
    }

    void process (juce::AudioBuffer<float>& buffer, const FxSettings& s)
    {
        const int n = buffer.getNumSamples();
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);

        // Low End: the low band is taken out here, kept away from the rack and added back after it.
        const bool split = appliedLowOn;
        if (split)
        {
            if (std::abs (s.lowXover - lowXoverNow) > 0.5f) { lowXoverNow = s.lowXover; lowEnd.setCutoffFrequency (juce::jlimit (30.0f, 400.0f, lowXoverNow)); }
            auto* lL = lowBuf.getWritePointer (0);
            auto* lR = lowBuf.getWritePointer (1);
            for (int i = 0; i < n; ++i)
            {
                float lo, hi;
                lowEnd.processSample (0, L[i], lo, hi); lL[i] = lo; L[i] = hi;
                lowEnd.processSample (1, R[i], lo, hi); lR[i] = lo; R[i] = hi;
            }
        }

        // A new order (or switching Low End) takes effect between blocks: this block fades out on the old setup
        // and the next fades in on the new one, so a change while playing dips for a moment instead of clicking.
        const bool reordering = s.order != applied || s.lowOn != appliedLowOn;
        for (auto id : applied) runEffect (id, buffer, L, R, n, s);
        if (split) processLowBand (L, R, n, s);
        else lastLowRms = lastHighRms = 0.0f;
        if (reordering)
        {
            for (int i = 0; i < n; ++i) { const float g = 1.0f - (float) (i + 1) / (float) n; L[i] *= g; R[i] *= g; }
            applied = s.order;
            if (s.lowOn != appliedLowOn) { appliedLowOn = s.lowOn; lowEnd.reset(); }
            fadeIn = true;
        }
        else if (fadeIn)
        {
            for (int i = 0; i < n; ++i) { const float g = (float) (i + 1) / (float) n; L[i] *= g; R[i] *= g; }
            fadeIn = false;
        }

        if (std::abs (s.width - 1.0f) > 0.001f)
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (L[i] + R[i]), sd = 0.5f * (L[i] - R[i]) * s.width;
                L[i] = m + sd;
                R[i] = m - sd;
            }
        if (s.monoBass) monoBass (L, R, n);
    }

    // For the Low End widget: how much energy each band carried in the last block (RMS).
    float lastLowRms = 0, lastHighRms = 0;

private:
    FxOrder applied = defaultFxOrder();
    bool fadeIn = false, appliedLowOn = false;
    juce::dsp::LinkwitzRileyFilter<float> lowEnd;
    juce::AudioBuffer<float> lowBuf;
    float lowXoverNow = 120.0f, lowGainSmoothed = 1.0f;
    double duckBeats = 0;

    // The low band: optional mono, warmth, level and a tempo-synced duck (a sidechain pump without a sidechain).
    void processLowBand (float* L, float* R, int n, const FxSettings& s)
    {
        auto* lL = lowBuf.getWritePointer (0);
        auto* lR = lowBuf.getWritePointer (1);
        static const double periods[] = { 1.0, 0.5, 2.0, 4.0, 0.25 };
        const double period = periods[juce::jlimit (0, 4, s.lowDuckRate)];
        const double beatsPerSample = s.bpm / 60.0 / sr;
        const float target = juce::Decibels::decibelsToGain (s.lowLevelDb);
        const float drive = s.lowDrive * s.lowDrive * 6.0f;
        const float driveNorm = 1.0f / (1.0f + drive * 0.35f);
        const float relSec = juce::jmax (0.01f, s.lowDuckRelease);
        double beat = s.playing ? s.ppq : duckBeats;
        double eL = 0, eH = 0;
        for (int i = 0; i < n; ++i)
        {
            float a = lL[i], b = lR[i];
            if (s.lowMono) a = b = 0.5f * (a + b);
            if (drive > 0.001f) { a = std::tanh (a * (1.0f + drive)) * driveNorm; b = std::tanh (b * (1.0f + drive)) * driveNorm; }
            lowGainSmoothed += (target - lowGainSmoothed) * 0.002f;
            float g = lowGainSmoothed;
            if (s.lowDuck > 0.001f)
            {
                const double tBeats = beat - std::floor (beat / period) * period;
                const float t = (float) (tBeats * 60.0 / s.bpm);
                const float env = t < 0.003f ? t / 0.003f : std::exp (-(t - 0.003f) / relSec);
                g *= 1.0f - s.lowDuck * env;
            }
            beat += beatsPerSample;
            eH += (double) L[i] * L[i] + (double) R[i] * R[i];
            a *= g; b *= g;
            eL += (double) a * a + (double) b * b;
            L[i] += a;
            R[i] += b;
        }
        duckBeats = s.playing ? beat : duckBeats + beatsPerSample * n;
        lastLowRms = (float) std::sqrt (eL / (2.0 * juce::jmax (1, n)));
        lastHighRms = (float) std::sqrt (eH / (2.0 * juce::jmax (1, n)));
    }

    void runEffect (int id, juce::AudioBuffer<float>& buffer, float* L, float* R, int n, const FxSettings& s)
    {
        switch (id)
        {
            case FxDist:
                if (s.distOn && s.distMix > 0.001f) distortion (buffer, s);
                break;
            case FxTape:
                if (s.tapeOn && (s.tapeWobble > 0.001f || s.tapeNoise > 0.001f || s.tapeSat > 0.001f))
                    tape.process (L, R, n, s.tapeWobble, s.tapeNoise, s.tapeSat);
                break;
            case FxOtt:
                if (s.ottOn && s.ott > 0.001f) ott (L, R, n, s.ott);
                break;
            case FxPitch:
                if (s.pitchOn && s.pitchMix > 0.001f) shifter.process (L, R, n, s.pitchSemis, s.pitchMix);
                break;
            case FxChorus:
                if (s.chorusOn && s.chorusMix > 0.001f)
                {
                    // Classic is the original; ensemble is deeper and slower; dimension is a fixed shallow wobble.
                    const float rate = s.chorusMode == 1 ? s.chorusRate * 0.6f : s.chorusMode == 2 ? 0.4f : s.chorusRate;
                    chorus.setRate (juce::jlimit (0.01f, 20.0f, rate));
                    chorus.setDepth (s.chorusMode == 1 ? 0.65f : s.chorusMode == 2 ? 0.22f : 0.35f);
                    chorus.setCentreDelay (s.chorusMode == 1 ? 14.0f : s.chorusMode == 2 ? 5.0f : 7.0f);
                    chorus.setFeedback (s.chorusMode == 1 ? 0.12f : 0.0f);
                    chorus.setMix (s.chorusMix * (s.chorusMode == 2 ? 0.35f : 0.5f));
                    juce::dsp::AudioBlock<float> block (buffer);
                    chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
                }
                break;
            case FxFlanger:
                if (s.flangerOn && s.flangerMix > 0.001f)
                    flanger.process (L, R, n, s.flangerRate, s.flangerDepth, s.flangerFeedback, s.flangerMix);
                break;
            case FxFilter:
                if (s.fxFilterOn && (s.fxFilterFreq < 19000.0f || s.fxFilterDepth > 0.001f || s.fxFilterType != 0))
                    fxFilter.process (L, R, n, s.fxFilterType, s.fxFilterFreq, s.fxFilterRes, s.fxFilterDepth, s.fxFilterRate, s.ppq, s.bpm, s.playing);
                break;
            case FxGate:
                if (s.gateOn && (s.gateDepth > 0.001f || s.panDepth > 0.001f))
                    gate.process (L, R, n, s.ppq, s.bpm, s.playing, s.gateDepth, s.gateRate, s.gatePattern, s.gateShape, s.panDepth, s.panRate);
                break;
            case FxDelay:
                if (s.delayOn && s.delayMix > 0.001f) delay (L, R, n, s);
                else delaySmoothed = -1;
                break;
            case FxReverb:
                if (s.reverbOn && s.reverbMix > 0.001f) reverb.process (L, R, n, s.reverbSize, s.reverbMix, s.shimmer, s.reverbMode);
                break;
            case FxEq:
                if (s.eqOn && (std::abs (s.eqLow) > 0.05f || std::abs (s.eqHigh) > 0.05f)) eq (L, R, n, s);
                break;
            default: break;
        }
    }

    double sr = 44100.0;
    juce::dsp::Oversampling<float> oversampler { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true }; // 4x for the distortion
    juce::dsp::Chorus<float> chorus;
    ab::dsp::Flanger flanger;
    ab::dsp::TapeDegrade tape;
    ab::dsp::GateAndPan gate;
    ab::dsp::FxFilter fxFilter;
    ab::dsp::PitchShifter shifter;
    int grainWrite = 0;
    std::array<float, 4> grainPos {}, grainRate {};
    std::array<int, 4> grainLeft {};
    int grainTimer = 0;
    juce::uint32 grainRng = 0x51f2c3u;
    SpaceReverb reverb;
    juce::dsp::LinkwitzRileyFilter<float> lowSplit, highSplit;
    float ottEnv[3] {};
    std::vector<float> delayLine[2];
    int delayWrite = 0;
    float delaySmoothed = -1, reversePos = 0;
    float delayFilt[4] {};
    juce::AudioBuffer<float> scratch;
    float dcX[2] {}, dcY[2] {};
    float sideHp[2] {};
    float crushHold[2] {};
    juce::dsp::IIR::Filter<float> eqLowF[2], eqHighF[2];
    float lastEqLow = 1000.0f, lastEqHigh = 1000.0f;
    int crushCount = 0;

    static inline float shape (int type, float x, float g)
    {
        switch (type)
        {
            case DistHard:    return juce::jlimit (-1.0f, 1.0f, x * g);
            case DistFold:    return std::sin (x * g * 1.3f);
            case Dist808:
            {
                const float v = juce::jlimit (-1.5f, 1.5f, x * g);
                return (v - v * v * v * (4.0f / 27.0f)) * 1.1f + 0.08f * v * v;
            }
            case DistRectify:
            {
                const float t = std::tanh (x * g);
                return 0.45f * t + 0.8f * std::abs (t) - 0.3f;
            }
            default: // tube: asymmetric so it adds even harmonics
                return std::tanh (x * g + 0.2f) - 0.19738f;
        }
    }

    void distortion (juce::AudioBuffer<float>& buffer, const FxSettings& s)
    {
        const int n = buffer.getNumSamples();
        const float drive = s.distDrive;
        const float g = 1.0f + drive * drive * 30.0f;
        const float comp = 1.0f / (1.0f + drive * 0.8f);
        scratch.makeCopyOf (buffer, true);

        if (s.distType == DistCrush)
        {
            const float levels = std::exp2 (12.0f - drive * 9.0f);
            const int hold = 1 + (int) (drive * drive * 24.0f);
            for (int i = 0; i < n; ++i)
            {
                if (crushCount++ % hold == 0)
                    for (int c = 0; c < 2; ++c)
                        crushHold[c] = std::round (buffer.getSample (c, i) * levels) / levels;
                for (int c = 0; c < 2; ++c)
                    buffer.setSample (c, i, crushHold[c]);
            }
        }
        else
        {
            juce::dsp::AudioBlock<float> block (buffer);
            auto up = oversampler.processSamplesUp (block);
            for (size_t c = 0; c < up.getNumChannels(); ++c)
            {
                auto* d = up.getChannelPointer (c);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                    d[i] = shape (s.distType, d[i], g) * comp;
            }
            oversampler.processSamplesDown (block);
        }

        // DC blocker (rectify/tube leave an offset), then wet/dry.
        const float R = 0.9975f;
        for (int c = 0; c < 2; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            const auto* dry = scratch.getReadPointer (c);
            for (int i = 0; i < n; ++i)
            {
                const float y = d[i] - dcX[c] + R * dcY[c];
                dcX[c] = d[i];
                dcY[c] = y;
                d[i] = dry[i] + (y - dry[i]) * s.distMix;
            }
        }
    }

    // Three bands, each pushed towards a target level from both sides (upward + downward), like OTT.
    void ott (float* L, float* R, int n, float amount)
    {
        const float att = std::exp (-1.0f / (0.003f * (float) sr));
        const float rel = std::exp (-1.0f / (0.08f * (float) sr));
        const float target = 0.18f;
        const float depth = amount * 0.85f;
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            float band[2][3];
            for (int c = 0; c < 2; ++c)
            {
                const float x = ch[c][i];
                const float low = lowSplit.processSample (c, x);
                const float rest = x - low;
                const float mid = highSplit.processSample (c, rest);
                band[c][0] = low;
                band[c][1] = mid;
                band[c][2] = rest - mid;
            }
            for (int b = 0; b < 3; ++b)
            {
                const float lvl = juce::jmax (std::abs (band[0][b]), std::abs (band[1][b]));
                const float coef = lvl > ottEnv[b] ? att : rel;
                ottEnv[b] = lvl + coef * (ottEnv[b] - lvl);
                const float env = juce::jmax (ottEnv[b], 1.0e-4f);
                float gain = std::pow (target / env, depth);
                gain = juce::jlimit (0.1f, 8.0f, gain);
                const float bandGain = b == 2 ? 1.15f : 1.0f;
                for (int c = 0; c < 2; ++c)
                    band[c][b] *= 1.0f + (gain * bandGain - 1.0f) * amount;
            }
            for (int c = 0; c < 2; ++c)
                ch[c][i] = band[c][0] + band[c][1] + band[c][2];
        }
    }

    void delay (float* L, float* R, int n, const FxSettings& s)
    {
        const int size = (int) delayLine[0].size();
        const float target = juce::jlimit (1.0f, (float) size - 4.0f, (float) (delayTimeBeats (s.delayTime) * 60.0 / juce::jmax (40.0f, s.bpm) * sr));
        if (delaySmoothed < 0) delaySmoothed = target;
        const float hpC = std::exp (-juce::MathConstants<float>::twoPi * 250.0f / (float) sr);
        const float lpC = std::exp (-juce::MathConstants<float>::twoPi * (800.0f * std::pow (18.0f, s.delayTone)) / (float) sr);
        const float fb = juce::jlimit (0.0f, 0.95f, s.delayFeedback);

        for (int i = 0; i < n; ++i)
        {
            delaySmoothed += (target - delaySmoothed) * 0.0005f;
            float rp = (float) delayWrite - delaySmoothed;
            if (rp < 0) rp += (float) size;
            const int i0 = (int) rp;
            const int i1 = (i0 + 1) % size;
            const float fr = rp - (float) i0;
            const float tapL = delayLine[0][(size_t) i0] + fr * (delayLine[0][(size_t) i1] - delayLine[0][(size_t) i0]);
            const float tapR = delayLine[1][(size_t) i0] + fr * (delayLine[1][(size_t) i1] - delayLine[1][(size_t) i0]);

            // Reverse and granular read the same buffer in a different order; the feedback path is shared.
            float outL = tapL, outR = tapR;
            if (s.delayStyle == 1)
            {
                // Reverse: play the last delay-length window backwards, so notes swim in tail-first.
                const float d = delaySmoothed;
                reversePos += 1.0f;
                if (reversePos >= d) reversePos -= d;
                float rrp = (float) delayWrite - (d - reversePos);
                if (rrp < 0) rrp += (float) size;
                const int r0 = (int) rrp, r1 = (r0 + 1) % size;
                const float rf = rrp - (float) r0;
                // fade the seam so the wrap doesn't click
                const float edge = juce::jmin (reversePos, d - reversePos) / juce::jmax (1.0f, d * 0.08f);
                const float w = juce::jlimit (0.0f, 1.0f, edge);
                outL = (delayLine[0][(size_t) r0] + rf * (delayLine[0][(size_t) r1] - delayLine[0][(size_t) r0])) * w;
                outR = (delayLine[1][(size_t) r0] + rf * (delayLine[1][(size_t) r1] - delayLine[1][(size_t) r0])) * w;
            }
            else if (s.delayStyle == 2)
            {
                // Granular: short windowed grains taken from anywhere in the buffer, some at half or double speed.
                if (--grainTimer <= 0)
                {
                    grainTimer = (int) (0.045 * sr);
                    for (size_t gi = 0; gi < grainPos.size(); ++gi)
                        if (grainLeft[gi] <= 0)
                        {
                            grainRng ^= grainRng << 13; grainRng ^= grainRng >> 17; grainRng ^= grainRng << 5;
                            const float r01 = (float) grainRng / 4294967295.0f;
                            grainLeft[gi] = (int) (0.12 * sr);
                            grainPos[gi] = delaySmoothed * (0.1f + 0.9f * r01);
                            grainRate[gi] = r01 < 0.25f ? 0.5f : r01 > 0.85f ? 2.0f : 1.0f;
                            break;
                        }
                }
                float gl = 0, gr = 0;
                for (size_t gi = 0; gi < grainPos.size(); ++gi)
                {
                    if (grainLeft[gi] <= 0) continue;
                    const float life = (float) grainLeft[gi] / (float) (0.12 * sr);
                    const float win = std::sin (juce::MathConstants<float>::pi * (1.0f - life));
                    float grp = (float) delayWrite - grainPos[gi];
                    while (grp < 0) grp += (float) size;
                    const int g0 = (int) grp, g1 = (g0 + 1) % size;
                    const float gf = grp - (float) g0;
                    gl += win * (delayLine[0][(size_t) g0] + gf * (delayLine[0][(size_t) g1] - delayLine[0][(size_t) g0]));
                    gr += win * (delayLine[1][(size_t) g0] + gf * (delayLine[1][(size_t) g1] - delayLine[1][(size_t) g0]));
                    grainPos[gi] -= grainRate[gi] - 1.0f;
                    --grainLeft[gi];
                }
                outL = gl * 0.7f;
                outR = gr * 0.7f;
            }

            // Ping-pong: the mono input enters left, each repeat swaps sides. Band-limited like a dub delay.
            const float in = (L[i] + R[i]) * 0.5f;
            float fbL = (s.delayPing ? tapR : tapL) * fb, fbR = (s.delayPing ? tapL : tapR) * fb;
            for (int c = 0; c < 2; ++c)
            {
                float& v = c == 0 ? fbL : fbR;
                delayFilt[c] = v + lpC * (delayFilt[c] - v);         // lowpass
                delayFilt[c + 2] = delayFilt[c] + hpC * (delayFilt[c + 2] - delayFilt[c]);
                v = std::tanh (delayFilt[c] - delayFilt[c + 2]);    // highpass + soft limit
            }
            delayLine[0][(size_t) delayWrite] = (s.delayPing ? in : L[i]) + fbL;
            delayLine[1][(size_t) delayWrite] = (s.delayPing ? 0.0f : R[i]) + fbR;
            delayWrite = (delayWrite + 1) % size;

            L[i] += outL * s.delayMix;
            R[i] += outR * s.delayMix;
        }
    }

    // Low shelf at 120 Hz and high shelf at 5 kHz, +-12 dB.
    void eq (float* L, float* R, int n, const FxSettings& s)
    {
        if (s.eqLow != lastEqLow)
        {
            auto c = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 120.0f, 0.7f, juce::Decibels::decibelsToGain (s.eqLow));
            for (auto& f : eqLowF) f.coefficients = c;
            lastEqLow = s.eqLow;
        }
        if (s.eqHigh != lastEqHigh)
        {
            auto c = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 5000.0f, 0.7f, juce::Decibels::decibelsToGain (s.eqHigh));
            for (auto& f : eqHighF) f.coefficients = c;
            lastEqHigh = s.eqHigh;
        }
        for (int i = 0; i < n; ++i)
        {
            L[i] = eqHighF[0].processSample (eqLowF[0].processSample (L[i]));
            R[i] = eqHighF[1].processSample (eqLowF[1].processSample (R[i]));
        }
    }

    // High-pass the side signal at ~120 Hz so everything below is mono.
    void monoBass (float* L, float* R, int n)
    {
        const float c = std::exp (-juce::MathConstants<float>::twoPi * 120.0f / (float) sr);
        for (int i = 0; i < n; ++i)
        {
            const float m = (L[i] + R[i]) * 0.5f, sd = (L[i] - R[i]) * 0.5f;
            sideHp[0] = sd + c * (sideHp[0] - sd);
            sideHp[1] = sideHp[0] + c * (sideHp[1] - sideHp[0]);
            const float hs = sd - sideHp[1];
            L[i] = m + hs;
            R[i] = m - hs;
        }
    }
};

} // namespace ab
