#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <array>

// The sampler: one imported sample played by every voice, alongside the oscillators.
// The sample itself is decoded on the message thread into an immutable SampleData; the audio thread only
// ever reads through a pointer the processor keeps alive (see HypernovaAudioProcessor::loadSample).
namespace ab
{

struct SampleData
{
    juce::String name;
    double rate = 44100.0;
    int length = 0;                  // frames
    std::vector<float> l, r;         // length + 4 frames: guard samples for interpolation at both ends
    float rootGuess = -1.0f;         // detected pitch as a MIDI note, or -1 when there's no clear pitch
    int onset = 0;                   // first frame above the noise floor

    // Cubic (Catmull-Rom) read at a fractional frame; outside the sample reads as silence.
    inline void read (double pos, float& outL, float& outR) const
    {
        if (pos < 0.0 || pos >= (double) length) { outL = outR = 0.0f; return; }
        const int i = (int) pos;
        const float t = (float) (pos - (double) i);
        auto cubic = [t] (const float* p) // p[0..3] = frames i-1, i, i+1, i+2
        {
            const float a = -0.5f * p[0] + 1.5f * p[1] - 1.5f * p[2] + 0.5f * p[3];
            const float b = p[0] - 2.5f * p[1] + 2.0f * p[2] - 0.5f * p[3];
            const float c = -0.5f * p[0] + 0.5f * p[2];
            return ((a * t + b) * t + c) * t + p[1];
        };
        // Stored with two guard frames in front, so frame i lives at index i + 2.
        outL = cubic (l.data() + i + 1);
        outR = cubic (r.data() + i + 1);
    }
};

// Chop Lab: a sample cut into slices, one per key. The edges are positions in the sample (0..1), sorted,
// with a slice between each pair, so `edges - 1` slices.
constexpr int MaxSlices = 32;
struct Slices
{
    int edges = 0;                                   // 0 or 2..MaxSlices+1
    std::array<float, MaxSlices + 1> at {};
    int count() const { return juce::jmax (0, edges - 1); }
    bool any() const { return edges >= 2; }
};

enum SampleLoop { LoopOff, LoopForward, LoopPingPong, NumLoopModes };
inline juce::StringArray sampleLoopNames() { return { "No loop", "Loop", "Ping-pong" }; }

struct SamplerSettings
{
    bool on = false, track = true, reverse = false, toFilter = true;
    int bus = 0;   // which bus the sampler plays into
    // Chop Lab: each key from chopRoot up plays its own slice, at the sample's own speed.
    bool chop = false, chopHold = false;   // hold: the slice plays while the key is down, otherwise it plays out
    int chopRoot = 60;
    Slices slices;
    const SampleData* data = nullptr;
    int root = 60, loop = LoopOff;
    float level = 0.8f, pan = 0, semi = 0, fine = 0;          // fine in cents
    float start = 0, end = 1, loopStart = 0.25f, loopEnd = 1; // 0..1 of the sample
    float a = 0.001f, d = 0.3f, s = 1.0f, r = 0.2f;
};

namespace dsp
{
    // One voice's playhead. Knows nothing about envelopes or levels: it turns a pitch into sample frames.
    struct SamplePlayer
    {
        double pos = 0;
        int dir = 1;
        bool done = true;

        int slice = -1;   // which slice this note plays, or -1 for the whole sample between the flags

        void start (const SamplerSettings& s, int sliceIndex = -1)
        {
            slice = sliceIndex;
            done = s.data == nullptr || s.data->length < 8;
            // A key outside the slices has nothing to play.
            if (! done && s.chop && s.slices.any() && (sliceIndex < 0 || sliceIndex >= s.slices.count())) done = true;
            if (done) return;
            double a, b;
            region (s, slice, a, b);
            dir = s.reverse ? -1 : 1;
            pos = s.reverse ? b - 1.0 : a;
        }

        static void region (const SamplerSettings& s, int slice, double& a, double& b)
        {
            const double len = (double) s.data->length;
            if (s.chop && s.slices.any() && slice >= 0 && slice + 1 < s.slices.edges)
            {
                a = juce::jlimit (0.0, len - 8.0, (double) s.slices.at[(size_t) slice] * len);
                b = juce::jlimit (a + 8.0, len, (double) s.slices.at[(size_t) slice + 1] * len);
                return;
            }
            a = juce::jlimit (0.0, len - 8.0, (double) juce::jmin (s.start, s.end) * len);
            b = juce::jlimit (a + 8.0, len, (double) juce::jmax (s.start, s.end) * len);
        }

        // Adds one frame (already scaled by gainL/gainR) and advances by `inc` frames.
        inline void tick (const SamplerSettings& s, double inc, float gainL, float gainR, float& outL, float& outR)
        {
            if (done || s.data == nullptr) return;
            const auto& d = *s.data;
            double a, b;
            region (s, slice, a, b);
            const bool looping = s.loop != LoopOff && ! (s.chop && s.slices.any());   // a slice plays once
            double la = a, lb = b;
            if (looping)
            {
                la = juce::jlimit (a, b - 8.0, a + (double) juce::jmin (s.loopStart, s.loopEnd) * (b - a));
                lb = juce::jlimit (la + 8.0, b, a + (double) juce::jmax (s.loopStart, s.loopEnd) * (b - a));
            }

            float L, R;
            d.read (pos, L, R);
            // Forward loops crossfade into the loop start over the last few ms so the seam doesn't click.
            if (looping && s.loop == LoopForward && dir > 0)
            {
                const double xf = juce::jmin (0.012 * d.rate, (lb - la) * 0.5, la);
                if (xf > 4.0 && pos > lb - xf)
                {
                    const float t = (float) ((pos - (lb - xf)) / xf);
                    float L2, R2;
                    d.read (pos - (lb - la), L2, R2);
                    L = L + (L2 - L) * t;
                    R = R + (R2 - R) * t;
                }
            }
            // A short fade at a one-shot's end: stopping mid-waveform would click.
            if (! looping)
            {
                const double left = dir > 0 ? b - pos : pos - a;
                const double fade = 0.003 * d.rate;
                if (left < fade) { const float g = (float) juce::jmax (0.0, left / fade); L *= g; R *= g; }
            }
            outL += L * gainL;
            outR += R * gainR;

            pos += inc * dir;
            if (looping)
            {
                if (s.loop == LoopForward)
                {
                    if (dir > 0 && pos >= lb) pos -= (lb - la);
                    else if (dir < 0 && pos < la) pos += (lb - la);
                }
                else
                {
                    if (dir > 0 && pos >= lb) { pos = lb - (pos - lb); dir = -1; }
                    else if (dir < 0 && pos < la) { pos = la + (la - pos); dir = 1; }
                }
                // The loop was moved while playing: bring the playhead back inside.
                if (pos < a || pos >= b) pos = juce::jlimit (la, lb - 1.0, pos);
            }
            else if (pos >= b || pos < a) done = true;
        }
    };

    // Where a recording's hits are: the loudness rises sharply, then doesn't rise again for a moment. Used
    // to cut a break into slices without doing it by hand. Returns positions in 0..1, always starting at 0.
    inline std::vector<float> findOnsets (const std::vector<float>& mono, double rate, int maxCount)
    {
        std::vector<float> out { 0.0f };
        const int n = (int) mono.size();
        const int hop = juce::jmax (64, (int) (0.005 * rate));       // 5 ms steps
        const int window = juce::jmax (hop, (int) (0.02 * rate));    // 20 ms of sound per step
        if (n < window * 4 || maxCount < 2) return out;

        std::vector<float> energy;
        energy.reserve ((size_t) (n / hop + 1));
        for (int i = 0; i + window < n; i += hop)
        {
            double e = 0;
            for (int k = 0; k < window; ++k) e += (double) mono[(size_t) (i + k)] * mono[(size_t) (i + k)];
            energy.push_back ((float) std::sqrt (e / window));
        }
        const int steps = (int) energy.size();
        if (steps < 8) return out;

        // How much louder each step is than the one before it: a hit is a peak in that.
        std::vector<float> rise ((size_t) steps, 0.0f);
        for (int i = 1; i < steps; ++i) rise[(size_t) i] = juce::jmax (0.0f, energy[(size_t) i] - energy[(size_t) i - 1]);
        double mean = 0;
        for (float v : rise) mean += v;
        mean /= steps;
        double spread = 0;
        for (float v : rise) spread += (v - mean) * (v - mean);
        spread = std::sqrt (spread / steps);
        float loudest = 0;
        for (float v : rise) loudest = juce::jmax (loudest, v);
        if (loudest < 1.0e-6f) return out;
        const float threshold = juce::jmax ((float) (mean + 1.2 * spread), loudest * 0.15f);

        const int neighbourhood = juce::jmax (2, (int) (0.02 * rate / hop));   // the peak of its own 40 ms
        const int apart = juce::jmax (neighbourhood, (int) (0.06 * rate / hop)); // no two hits within 60 ms
        std::vector<std::pair<float, int>> hits;
        int last = -apart;
        for (int i = 1; i < steps; ++i)
        {
            if (rise[(size_t) i] < threshold || i - last < apart) continue;
            bool peak = true;
            for (int k = juce::jmax (0, i - neighbourhood); k <= juce::jmin (steps - 1, i + neighbourhood); ++k)
                if (rise[(size_t) k] > rise[(size_t) i]) { peak = false; break; }
            if (! peak) continue;
            hits.push_back ({ rise[(size_t) i], i });
            last = i;
        }
        // Keep the strongest if there are more than there are keys for, then put them back in order.
        std::sort (hits.begin(), hits.end(), [] (auto& a, auto& b) { return a.first > b.first; });
        if ((int) hits.size() > maxCount - 1) hits.resize ((size_t) maxCount - 1);
        std::sort (hits.begin(), hits.end(), [] (auto& a, auto& b) { return a.second < b.second; });
        for (auto& [strength, step] : hits)
        {
            const float at = (float) ((double) step * hop / (double) n);
            if (at > 0.01f && at < 0.99f && at - out.back() > 0.01f) out.push_back (at);
        }
        return out;
    }

    // Pitch of a recording, as a MIDI note: normalised autocorrelation over a steady stretch after the attack.
    // Returns -1 when there's no convincing pitch (drums, noise).
    inline float detectPitch (const std::vector<float>& mono, double rate, int from)
    {
        const int n = (int) mono.size();
        const int win = juce::jmin ((int) (0.12 * rate), n - from - 1);
        const int minLag = (int) (rate / 1500.0), maxLag = juce::jmin ((int) (rate / 30.0), win / 2);
        if (win < 512 || maxLag <= minLag + 2) return -1.0f;
        const float* x = mono.data() + from;
        std::vector<double> nsdf ((size_t) maxLag + 1, 0.0);
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double ac = 0, m = 0;
            for (int i = 0; i + lag < win; ++i)
            {
                ac += (double) x[i] * x[i + lag];
                m += (double) x[i] * x[i] + (double) x[i + lag] * x[i + lag];
            }
            nsdf[(size_t) lag] = m > 1.0e-9 ? 2.0 * ac / m : 0.0;
        }
        // McLeod: the first peak that gets within 90% of the highest one.
        double best = 0;
        for (int lag = minLag; lag <= maxLag; ++lag) best = juce::jmax (best, nsdf[(size_t) lag]);
        if (best < 0.8) return -1.0f;
        for (int lag = minLag + 1; lag < maxLag; ++lag)
        {
            const double v = nsdf[(size_t) lag];
            if (v >= 0.9 * best && v >= nsdf[(size_t) lag - 1] && v >= nsdf[(size_t) lag + 1])
            {
                // Parabolic interpolation around the peak.
                const double a = nsdf[(size_t) lag - 1], c = nsdf[(size_t) lag + 1];
                const double denom = a - 2.0 * v + c; // negative at a peak
                const double shift = std::abs (denom) > 1.0e-12 ? juce::jlimit (-1.0, 1.0, 0.5 * (a - c) / denom) : 0.0;
                const double hz = rate / ((double) lag + shift);
                return (float) (69.0 + 12.0 * std::log2 (hz / 440.0));
            }
        }
        return -1.0f;
    }
}

} // namespace ab
