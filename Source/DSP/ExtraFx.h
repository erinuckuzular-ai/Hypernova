#pragma once

#include <juce_dsp/juce_dsp.h>

// The second half of the effect rack: movement (flanger, gate, auto-pan, swept filter), character
// (tape and vinyl wear), and pitch shifting. Each one costs nothing while its mix or depth is zero.
namespace ab::dsp
{

// Tempo-synced rates, matching the delay's list so the UI reads the same everywhere.
inline juce::StringArray syncRateNames() { return { "1 bar", "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32" }; }
inline double syncRateBeats (int i)
{
    static const double beats[] = { 4.0, 2.0, 1.0, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    return beats[juce::jlimit (0, 8, i)];
}

//==============================================================================
// Flanger: a short modulated delay with feedback. Negative feedback gives the hollow jet-plane sound.
class Flanger
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& l : line) l.assign ((size_t) (0.03 * sr) + 8, 0.0f);
        write = 0;
        phase = 0;
    }

    void reset() { for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f); }

    void process (float* L, float* R, int n, float rateHz, float depth, float feedback, float mix)
    {
        const int size = (int) line[0].size();
        const float fb = juce::jlimit (-0.95f, 0.95f, feedback);
        const double inc = rateHz / sr;
        for (int i = 0; i < n; ++i)
        {
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
            float* ch[2] = { &L[i], &R[i] };
            for (int c = 0; c < 2; ++c)
            {
                // The two channels sweep a quarter cycle apart, which widens the sound.
                const double p = phase + (c == 0 ? 0.0 : 0.25);
                const float lfo = 0.5f + 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * (p - std::floor (p)));
                const float delaySamples = (float) (sr * (0.0005 + 0.006 * depth * lfo)) + 1.0f;
                float rp = (float) write - delaySamples;
                if (rp < 0) rp += (float) size;
                const int i0 = (int) rp, i1 = (i0 + 1) % size;
                const float fr = rp - (float) i0;
                const float tap = line[c][(size_t) i0] + fr * (line[c][(size_t) i1] - line[c][(size_t) i0]);
                line[c][(size_t) write] = *ch[c] + tap * fb;
                *ch[c] += tap * mix;
            }
            write = (write + 1) % size;
        }
    }

private:
    double sr = 44100.0, phase = 0;
    std::vector<float> line[2];
    int write = 0;
};

//==============================================================================
// Tape and vinyl wear: pitch wobble (wow and flutter), a soft saturating stage and surface noise.
class TapeDegrade
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& l : line) l.assign ((size_t) (0.05 * sr) + 8, 0.0f);
        write = 0;
        wowPhase = flutterPhase = 0;
        crackle = 0;
    }

    void reset() { for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f); }

    void process (float* L, float* R, int n, float wobble, float noise, float saturation)
    {
        const int size = (int) line[0].size();
        const double wowInc = 0.7 / sr, flutInc = 6.3 / sr;
        const float drive = 1.0f + saturation * 8.0f;
        const float norm = 1.0f / std::tanh (drive);
        for (int i = 0; i < n; ++i)
        {
            wowPhase += wowInc;
            flutterPhase += flutInc;
            if (wowPhase >= 1.0) wowPhase -= 1.0;
            if (flutterPhase >= 1.0) flutterPhase -= 1.0;
            const float wow = (float) std::sin (juce::MathConstants<double>::twoPi * wowPhase);
            const float flutter = (float) std::sin (juce::MathConstants<double>::twoPi * flutterPhase);
            const float offset = (float) (sr * 0.012) + (float) (sr * 0.008) * wobble * (wow + 0.35f * flutter);

            // Surface noise: steady hiss plus the occasional crackle.
            float hiss = 0.0f;
            if (noise > 0.0001f)
            {
                rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
                const float w = (float) rngState * (2.0f / 4294967295.0f) - 1.0f;
                if (w > 0.9993f) crackle = 1.0f;
                crackle *= (float) std::exp (-900.0 / sr);
                hiss = (w * 0.06f + w * crackle * 1.4f) * noise;
            }

            float* ch[2] = { &L[i], &R[i] };
            for (int c = 0; c < 2; ++c)
            {
                line[c][(size_t) write] = *ch[c];
                float rp = (float) write - offset;
                if (rp < 0) rp += (float) size;
                const int i0 = (int) rp, i1 = (i0 + 1) % size;
                const float fr = rp - (float) i0;
                float v = line[c][(size_t) i0] + fr * (line[c][(size_t) i1] - line[c][(size_t) i0]);
                if (saturation > 0.0001f) v = std::tanh (v * drive) * norm;
                *ch[c] = v + hiss;
            }
            write = (write + 1) % size;
        }
    }

private:
    double sr = 44100.0, wowPhase = 0, flutterPhase = 0;
    std::vector<float> line[2];
    int write = 0;
    float crackle = 0;
    juce::uint32 rngState = 0x1234567u;
};

//==============================================================================
// Trance gate and auto-pan, both locked to the song. The gate runs a 16-step pattern; SHAPE softens its edges.
class GateAndPan
{
public:
    static juce::StringArray patternNames() { return { "Steady", "Off-beat", "Gallop", "Sixteenths", "Triplet feel" }; }
    static juce::uint16 patternMask (int pattern)
    {
        static const juce::uint16 patterns[] = { 0xFFFF, 0xAAAA, 0xCCDD, 0xFFFF, 0xDB6D };
        return patterns[juce::jlimit (0, 4, pattern)];
    }

    void prepare (double sampleRate) { sr = sampleRate; env = 1.0f; panPhase = 0; }

    void process (float* L, float* R, int n, double ppqStart, double bpm, bool playing,
                  float gateDepth, int gateRate, int pattern, float shape,
                  float panDepth, int panRate)
    {
        static const juce::uint16 patterns[] = { 0xFFFF, 0xAAAA, 0xCCDD, 0xFFFF, 0xDB6D };
        const juce::uint16 mask = patterns[juce::jlimit (0, 4, pattern)];
        const double beatsPerSample = juce::jmax (20.0, bpm) / 60.0 / sr;
        const double gateBeats = syncRateBeats (gateRate), panBeats = syncRateBeats (panRate);
        const float smooth = 1.0f - std::exp (-1.0f / ((0.0004f + 0.02f * shape) * (float) sr));

        for (int i = 0; i < n; ++i)
        {
            const double beats = playing ? ppqStart + i * beatsPerSample : freeBeats + i * beatsPerSample;

            if (gateDepth > 0.0001f)
            {
                const int step = ((int) std::floor (beats / gateBeats)) & 15;
                const bool on = ((mask >> (15 - step)) & 1) != 0;
                env += ((on ? 1.0f : 0.0f) - env) * smooth;
                const float g = 1.0f - gateDepth + gateDepth * env;
                L[i] *= g;
                R[i] *= g;
            }

            if (panDepth > 0.0001f)
            {
                const double p = beats / (panBeats * 2.0);
                const float lfo = (float) std::sin (juce::MathConstants<double>::twoPi * (p - std::floor (p)));
                const float pan = lfo * panDepth;
                L[i] *= 1.0f - juce::jmax (0.0f, pan);
                R[i] *= 1.0f + juce::jmin (0.0f, pan);
            }
        }
        if (! playing) freeBeats += n * beatsPerSample;
        else freeBeats = ppqStart + n * beatsPerSample;
    }

private:
    double sr = 44100.0, freeBeats = 0, panPhase = 0;
    float env = 1.0f;
};

//==============================================================================
// A filter in the effect chain, optionally swept by a synced LFO. Separate from the synth's own filter,
// so it shapes the whole patch including its reverb and delay tails.
class FxFilter
{
public:
    enum Type { Low, High, Band, NumTypes };
    static juce::StringArray typeNames() { return { "Low Pass", "High Pass", "Band Pass" }; }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        reset();
    }

    void reset() { for (auto& s : state) s = 0; }

    void process (float* L, float* R, int n, int type, float freq, float res, float depth, int rate, double ppqStart, double bpm, bool playing)
    {
        const double beatsPerSample = juce::jmax (20.0, bpm) / 60.0 / sr;
        const double lfoBeats = syncRateBeats (rate) * 2.0;
        const float k = 2.0f - 1.9f * juce::jlimit (0.0f, 1.0f, res);
        for (int i = 0; i < n; ++i)
        {
            float f = freq;
            if (depth > 0.0001f)
            {
                const double beats = (playing ? ppqStart : freeBeats) + i * beatsPerSample;
                const double p = beats / lfoBeats;
                const float lfo = (float) std::sin (juce::MathConstants<double>::twoPi * (p - std::floor (p)));
                f *= std::exp2 (lfo * depth * 3.0f);
            }
            const float g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.0f, (float) sr * 0.45f, f) / (float) sr);
            for (int c = 0; c < 2; ++c)
            {
                float* x = c == 0 ? &L[i] : &R[i];
                float& ic1 = state[(size_t) (c * 2)];
                float& ic2 = state[(size_t) (c * 2 + 1)];
                const float a1 = 1.0f / (1.0f + g * (g + k));
                const float v1 = a1 * (*x - ic2 - ic1 * (g + k)) * g + ic1;  // band
                const float v2 = (v1 - ic1) * g + ic2;                        // low
                ic1 = 2.0f * v1 - ic1;
                ic2 = 2.0f * v2 - ic2;
                const float lo = v2, band = v1, hi = *x - k * v1 - v2;
                *x = type == Low ? lo : type == High ? hi : band;
            }
        }
        if (! playing) freeBeats += n * beatsPerSample;
        else freeBeats = ppqStart + n * beatsPerSample;
    }

private:
    double sr = 44100.0, freeBeats = 0;
    std::array<float, 4> state {};
};

//==============================================================================
// Pitch shifter: two taps running through a delay line half a window apart, crossfaded so the seam is hidden.
// Cheap and stable, the classic way to transpose without a phase vocoder.
class PitchShifter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        window = (float) (0.08 * sr);
        for (auto& l : line) l.assign ((size_t) (0.25 * sr) + 8, 0.0f);
        write = 0;
        phase = 0;
    }

    void reset() { for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f); }

    void process (float* L, float* R, int n, float semitones, float mix)
    {
        const int size = (int) line[0].size();
        const float ratio = std::exp2 (semitones / 12.0f);
        const float step = (ratio - 1.0f);
        for (int i = 0; i < n; ++i)
        {
            phase += step;
            if (phase >= window) phase -= window;
            if (phase < 0) phase += window;

            float* ch[2] = { &L[i], &R[i] };
            for (int c = 0; c < 2; ++c)
            {
                line[c][(size_t) write] = *ch[c];
                float sum = 0;
                for (int t = 0; t < 2; ++t)
                {
                    float p = phase + (t == 0 ? 0.0f : window * 0.5f);
                    if (p >= window) p -= window;
                    // Triangular crossfade: each tap fades in and out across the window.
                    const float gain = 1.0f - std::abs (2.0f * (p / window) - 1.0f);
                    float rp = (float) write - (window - p) - 2.0f;
                    while (rp < 0) rp += (float) size;
                    const int i0 = (int) rp, i1 = (i0 + 1) % size;
                    const float fr = rp - (float) i0;
                    sum += gain * (line[c][(size_t) i0] + fr * (line[c][(size_t) i1] - line[c][(size_t) i0]));
                }
                *ch[c] = *ch[c] * (1.0f - mix) + sum * mix;
            }
            write = (write + 1) % size;
        }
    }

private:
    double sr = 44100.0;
    float window = 2048.0f, phase = 0;
    std::vector<float> line[2];
    int write = 0;
};

} // namespace ab::dsp
