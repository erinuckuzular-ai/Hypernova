#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

// Event Horizon: hold a moment of the sound open. The input is taken apart into a spectrum every few
// milliseconds; pressing HOLD keeps the last frame and plays it back over and over with new phases, which
// turns anything — a chord, a word, a crash — into a pad that never moves. SHIFT transposes what is held,
// SPREAD smears it sideways across the spectrum, and BLUR softens how quickly it can change.
namespace ab::dsp
{

class Freeze
{
public:
    static constexpr int Order = 11;              // 2048-point window
    static constexpr int Size = 1 << Order;
    static constexpr int Hop = Size / 4;          // 75% overlap: smooth enough for a held pad
    static int latencySamples() { return Size - Hop; }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        input.assign ((size_t) Size, 0.0f);
        output.assign ((size_t) (Size * 2), 0.0f);
        window.assign ((size_t) Size, 0.0f);
        for (int i = 0; i < Size; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) Size);
        frame.assign ((size_t) Size * 2, 0.0f);
        held.assign ((size_t) Size / 2 + 1, 0.0f);
        smoothed.assign ((size_t) Size / 2 + 1, 0.0f);
        phase.assign ((size_t) Size / 2 + 1, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (input.begin(), input.end(), 0.0f);
        std::fill (output.begin(), output.end(), 0.0f);
        std::fill (held.begin(), held.end(), 0.0f);
        std::fill (smoothed.begin(), smoothed.end(), 0.0f);
        for (auto& p : phase) p = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        fill = 0;
        readAt = 0;
        holding = false;
    }

    struct Settings
    {
        bool hold = false;
        float blur = 0.3f;     // how slowly what is held is allowed to change
        float shift = 0;       // semitones the held spectrum is moved by
        float spread = 0;      // how far it smears sideways
        float mix = 0;         // what you hear of it against the sound going in
    };

    // Processes a block in place. The wet path is delayed by latencySamples(), which the host is told about.
    void process (float* L, float* R, int n, const Settings& s)
    {
        if (s.mix <= 0.0001f && ! holding && ! s.hold) { passThrough (L, R, n); return; }
        const float mix = juce::jlimit (0.0f, 1.0f, s.mix);
        for (int i = 0; i < n; ++i)
        {
            const float in = 0.5f * (L[i] + R[i]);
            input[(size_t) fill] = in;
            dry[(size_t) (dryAt % dry.size())] = { L[i], R[i] };
            ++dryAt;
            ++fill;
            if (fill >= Size)
            {
                analyse (s);
                // Slide the window on by one hop.
                std::memmove (input.data(), input.data() + Hop, (size_t) (Size - Hop) * sizeof (float));
                fill = Size - Hop;
            }
            const float wet = output[(size_t) (readAt % output.size())];
            output[(size_t) (readAt % output.size())] = 0.0f;
            ++readAt;
            // The dry sound is delayed to meet the wet one, so the two stay in step.
            const auto delayed = dry[(size_t) ((dryAt + dry.size() - (size_t) latencySamples() - 1) % dry.size())];
            L[i] = delayed.first + (wet - delayed.first) * mix;
            R[i] = delayed.second + (wet - delayed.second) * mix;
        }
    }

    bool isHolding() const { return holding; }
    const std::vector<float>& heldSpectrum() const { return held; }

private:
    // Doing nothing: the sound goes straight through, with no delay at all, so a unit sitting in the rack
    // with its mix down costs nothing and moves nothing. The window keeps filling, so HOLD still catches
    // whatever is playing the moment it goes down.
    void passThrough (float* L, float* R, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            dry[(size_t) (dryAt % dry.size())] = { L[i], R[i] };
            ++dryAt;
            input[(size_t) fill] = 0.5f * (L[i] + R[i]);
            if (++fill >= Size)
            {
                std::memmove (input.data(), input.data() + Hop, (size_t) (Size - Hop) * sizeof (float));
                fill = Size - Hop;
                keepFollowing();
            }
        }
    }

    // The spectrum of the window that just filled, kept ready for the moment HOLD goes down.
    void keepFollowing()
    {
        const int bins = Size / 2 + 1;
        std::fill (frame.begin(), frame.end(), 0.0f);
        for (int i = 0; i < Size; ++i) frame[(size_t) i] = input[(size_t) i] * window[(size_t) i];
        fft.performRealOnlyForwardTransform (frame.data(), true);
        for (int b = 0; b < bins; ++b)
        {
            const float re = frame[(size_t) (b * 2)], im = frame[(size_t) (b * 2 + 1)];
            smoothed[(size_t) b] = std::sqrt (re * re + im * im);
            held[(size_t) b] = smoothed[(size_t) b];
        }
        holding = false;
    }

    void analyse (const Settings& s)
    {
        const int bins = Size / 2 + 1;
        // Window and transform the frame that just filled up.
        std::fill (frame.begin(), frame.end(), 0.0f);
        for (int i = 0; i < Size; ++i) frame[(size_t) i] = input[(size_t) i] * window[(size_t) i];
        fft.performRealOnlyForwardTransform (frame.data(), true);

        const float keep = juce::jlimit (0.0f, 0.995f, 0.3f + 0.69f * juce::jlimit (0.0f, 1.0f, s.blur));
        if (! s.hold)
        {
            // Not holding: keep following the sound, so HOLD catches whatever is there at that moment.
            for (int b = 0; b < bins; ++b)
            {
                const float re = frame[(size_t) (b * 2)], im = frame[(size_t) (b * 2 + 1)];
                const float magnitude = std::sqrt (re * re + im * im);
                smoothed[(size_t) b] = magnitude + (smoothed[(size_t) b] - magnitude) * keep;
                held[(size_t) b] = smoothed[(size_t) b];
            }
            holding = false;
        }
        else if (! holding)
        {
            holding = true;   // the frame that was there when HOLD went down is the one that stays
        }

        // Rebuild a frame from what is held: the same magnitudes every time, with phases that move on, so
        // it sounds like the sound going on rather than a loop.
        std::fill (frame.begin(), frame.end(), 0.0f);
        const float semis = juce::jlimit (-24.0f, 24.0f, s.shift);
        const float ratio = std::pow (2.0f, semis / 12.0f);
        const int smear = juce::jlimit (0, 64, (int) (juce::jlimit (0.0f, 1.0f, s.spread) * 40.0f));
        for (int b = 1; b < bins - 1; ++b)
        {
            float magnitude = 0;
            if (std::abs (ratio - 1.0f) < 0.001f) magnitude = held[(size_t) b];
            else
            {
                // Read the held spectrum from where this bin came from, so the whole thing moves in pitch.
                const float from = (float) b / ratio;
                const int i0 = (int) from;
                if (i0 >= 1 && i0 < bins - 1)
                {
                    const float t = from - (float) i0;
                    magnitude = held[(size_t) i0] * (1.0f - t) + held[(size_t) (i0 + 1)] * t;
                }
            }
            if (smear > 0)
            {
                // Spread: take a little from the bins either side, which blurs pitch into texture.
                float sum = magnitude;
                float weight = 1.0f;
                for (int k = 1; k <= smear; k += 2)
                {
                    const float w = 1.0f - (float) k / (float) (smear + 1);
                    if (b - k >= 1) { sum += held[(size_t) (b - k)] * w; weight += w; }
                    if (b + k < bins) { sum += held[(size_t) (b + k)] * w; weight += w; }
                }
                magnitude = sum / weight;
            }
            // The phase moves on by what this bin's frequency would do in one hop, with a little wander so
            // it never sounds like a machine.
            phase[(size_t) b] += juce::MathConstants<float>::twoPi * (float) b * (float) Hop / (float) Size
                                 + (rng.nextFloat() - 0.5f) * 0.4f;
            if (phase[(size_t) b] > juce::MathConstants<float>::twoPi) phase[(size_t) b] -= juce::MathConstants<float>::twoPi;
            frame[(size_t) (b * 2)] = magnitude * std::cos (phase[(size_t) b]);
            frame[(size_t) (b * 2 + 1)] = magnitude * std::sin (phase[(size_t) b]);
        }
        fft.performRealOnlyInverseTransform (frame.data());

        // Overlap-add the new frame into the output, windowed again so the joins don't show.
        const float gain = 2.0f / 3.0f;   // Hann analysis + synthesis at 75% overlap
        for (int i = 0; i < Size; ++i)
            output[(size_t) ((readAt + (size_t) i) % output.size())] += frame[(size_t) i] * window[(size_t) i] * gain;
    }

    juce::dsp::FFT fft { Order };
    double sr = 44100.0;
    std::vector<float> input, output, window, frame, held, smoothed, phase;
    std::array<std::pair<float, float>, (size_t) Size * 2> dry {};
    size_t dryAt = 0, readAt = 0;
    int fill = 0;
    bool holding = false;
    juce::Random rng { 0x5eed };
};

} // namespace ab::dsp
