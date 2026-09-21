#pragma once

#include <juce_dsp/juce_dsp.h>
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

struct FxSettings
{
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
    void process (float* L, float* R, int n, float size, float mix, float shimmer)
    {
        static const float baseMs[8] = { 37.3f, 41.9f, 47.1f, 53.3f, 59.9f, 67.7f, 73.1f, 79.3f };
        static const float diffMs[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
        const float scale = 0.7f + 0.9f * size;
        const float rt60 = 0.4f * std::pow (40.0f, size);
        float len[8], gain[8];
        for (int i = 0; i < 8; ++i)
        {
            len[i] = baseMs[i] * scale * 0.001f * (float) sr;
            gain[i] = std::pow (10.0f, -3.0f * len[i] / (rt60 * (float) sr));
        }
        const float dampC = std::exp (-juce::MathConstants<float>::twoPi * (9000.0f - 3500.0f * size) / (float) sr);
        const int preDelay = (int) ((0.008f + 0.035f * size) * (float) sr);
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

        const int maxDelay = (int) (sampleRate * 4.5) + 8;
        for (auto& d : delayLine) d.assign ((size_t) maxDelay, 0.0f);
        delayWrite = 0;
        delaySmoothed = -1;
        for (auto& s : delayFilt) s = 0;

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
    }

    void process (juce::AudioBuffer<float>& buffer, const FxSettings& s)
    {
        const int n = buffer.getNumSamples();
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);

        if (s.distOn && s.distMix > 0.001f) distortion (buffer, s);
        if (s.ottOn && s.ott > 0.001f) ott (L, R, n, s.ott);

        if (s.chorusOn && s.chorusMix > 0.001f)
        {
            chorus.setRate (s.chorusRate);
            chorus.setMix (s.chorusMix * 0.5f);
            juce::dsp::AudioBlock<float> block (buffer);
            chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        if (s.delayOn && s.delayMix > 0.001f) delay (L, R, n, s);
        else delaySmoothed = -1;

        if (s.reverbOn && s.reverbMix > 0.001f) reverb.process (L, R, n, s.reverbSize, s.reverbMix, s.shimmer);

        if (s.eqOn && (std::abs (s.eqLow) > 0.05f || std::abs (s.eqHigh) > 0.05f)) eq (L, R, n, s);
        if (std::abs (s.width - 1.0f) > 0.001f)
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (L[i] + R[i]), sd = 0.5f * (L[i] - R[i]) * s.width;
                L[i] = m + sd;
                R[i] = m - sd;
            }
        if (s.monoBass) monoBass (L, R, n);
    }

private:
    double sr = 44100.0;
    juce::dsp::Oversampling<float> oversampler { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true }; // 4x for the distortion
    juce::dsp::Chorus<float> chorus;
    SpaceReverb reverb;
    juce::dsp::LinkwitzRileyFilter<float> lowSplit, highSplit;
    float ottEnv[3] {};
    std::vector<float> delayLine[2];
    int delayWrite = 0;
    float delaySmoothed = -1;
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

            L[i] += tapL * s.delayMix;
            R[i] += tapR * s.delayMix;
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
