#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <vector>

// Resonators: a struck or plucked thing that rings at the note you play. Whatever you send into it is the
// hit — a click of noise, a drum, a chopped sample, a whole oscillator — and what comes out is a string, a
// tube, a bell, a plate or a drum head ringing with it.
//
// Two kinds live here, behind one set of controls:
//  - String and Tube are waveguides: a delay line as long as one cycle of the note, with a damping filter
//    in the loop and an all-pass that spreads the higher partials out of tune the way a real string does.
//  - Bell, Plate and Drum are modal: a bank of resonators tuned to ratios that aren't whole numbers, which
//    is what makes them sound like metal and skin rather than a string.
namespace ab::dsp
{

class Resonator
{
public:
    enum Model { String, Tube, Bell, Plate, Drum, NumModels };
    static juce::StringArray modelNames() { return { "String", "Tube", "Bell", "Plate", "Drum" }; }

    struct Settings
    {
        int model = String;
        float freq = 220.0f;     // where it rings, in Hz
        float structure = 0.3f;  // how far from whole-number ratios the partials sit
        float bright = 0.6f;     // how much of the high end survives each pass
        float decay = 0.5f;      // how long it rings
        float position = 0.3f;   // where it is struck: thins out the partials that have a node there
        float mix = 0.0f;        // what you hear of it against the sound going in
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        line.assign ((size_t) juce::jmax (256, (int) (sr / 16.0) + 8), 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (line.begin(), line.end(), 0.0f);
        write = 0;
        damp = allpassX = allpassY = 0.0f;
        for (auto& m : modes) m = {};
        dcState = dcPrev = 0.0f;
    }

    // One sample in, one sample out, already mixed against the input.
    inline float tick (float in, const Settings& s)
    {
        if (s.mix <= 0.0001f) return in;
        const float wet = isModal (s.model) ? modal (in, s) : waveguide (in, s);
        return in + (wet - in) * juce::jlimit (0.0f, 1.0f, s.mix);
    }

    // The settings change per block, not per sample: the coefficients are worked out here.
    void update (const Settings& s)
    {
        const float f = juce::jlimit (20.0f, (float) (sr * 0.25), s.freq);
        delaySamples = juce::jlimit (4.0, (double) line.size() - 4.0, sr / f);

        // Decay: 0.05 s to 20 s, as a loop gain for the waveguide and a pole radius for each mode.
        seconds = 0.05f * std::pow (400.0f, juce::jlimit (0.0f, 1.0f, s.decay));
        loopGain = (float) std::pow (0.001, delaySamples / (seconds * sr));
        loopGain = juce::jlimit (0.0f, 0.9995f, loopGain);

        // Brightness: how much of each pass survives the damping filter in the loop.
        dampCoef = juce::jlimit (0.0f, 0.98f, 0.9f - 0.85f * juce::jlimit (0.0f, 1.0f, s.bright));

        // Stiffness: a first-order all-pass spreads the upper partials sharp, like a piano string or a bell.
        const float stiffness = juce::jlimit (0.0f, 1.0f, s.structure);
        allpassCoef = -0.7f * stiffness;

        if (isModal (s.model))
        {
            const auto ratios = ratiosFor (s.model);
            const float spread = juce::jlimit (0.0f, 1.0f, s.structure);
            for (int i = 0; i < NumModes; ++i)
            {
                // Structure at zero is a harmonic series; turned up, each partial moves to the model's own ratio.
                const float harmonic = (float) (i + 1);
                const float ratio = harmonic + (ratios[(size_t) i] - harmonic) * spread;
                const float hz = f * ratio;
                auto& m = modes[(size_t) i];
                m.on = hz > 20.0f && hz < sr * 0.45;
                if (! m.on) continue;
                const float w = juce::MathConstants<float>::twoPi * hz / (float) sr;
                // Higher partials die away sooner, and brightness decides how much sooner.
                const float life = seconds * std::pow (1.0f / ratio, 1.6f - 1.2f * juce::jlimit (0.0f, 1.0f, s.bright));
                m.radius = juce::jlimit (0.0f, 0.99995f, (float) std::exp (-1.0 / juce::jmax (0.001f, life) / sr));
                m.cosw = std::cos (w);
                m.sinw = std::sin (w);
                // Struck at `position`: a partial with a node there hardly rings at all.
                m.gain = std::abs (std::sin (juce::MathConstants<float>::pi * ratio * juce::jlimit (0.02f, 0.98f, s.position)))
                         / (0.6f + 0.9f * (float) i);
            }
        }
    }

    float ringingSeconds() const { return seconds; }

    // What the display draws: where the partials sit and how strongly each one rings.
    static std::vector<std::pair<float, float>> partials (const Settings& s)
    {
        std::vector<std::pair<float, float>> out;
        const bool modal = isModal (s.model);
        const auto ratios = ratiosFor (s.model);
        const float spread = juce::jlimit (0.0f, 1.0f, s.structure);
        for (int i = 0; i < NumModes; ++i)
        {
            const float harmonic = (float) (i + 1);
            const float ratio = modal ? harmonic + (ratios[(size_t) i] - harmonic) * spread
                                      : harmonic * (1.0f + spread * 0.06f * harmonic);   // a stiff string goes sharp
            const float node = std::abs (std::sin (juce::MathConstants<float>::pi * ratio * juce::jlimit (0.02f, 0.98f, s.position)));
            const float tilt = std::pow (1.0f / ratio, 1.4f - 1.1f * juce::jlimit (0.0f, 1.0f, s.bright));
            out.push_back ({ ratio, juce::jlimit (0.0f, 1.0f, node * tilt) });
        }
        return out;
    }

private:
    static constexpr int NumModes = 10;
    static bool isModal (int model) { return model >= Bell; }

    // What each model's partials are tuned to. A string's are whole numbers; these are not, which is what
    // makes them sound struck rather than plucked.
    static std::array<float, NumModes> ratiosFor (int model)
    {
        switch (model)
        {
            case Bell:  return { 1.0f, 2.00f, 2.40f, 3.00f, 4.50f, 5.33f, 6.67f, 8.00f, 9.20f, 11.0f };   // minor-third bell
            case Plate: return { 1.0f, 1.59f, 2.14f, 2.30f, 2.65f, 2.92f, 3.16f, 3.50f, 4.06f, 4.60f };
            case Drum:  return { 1.0f, 1.59f, 2.14f, 2.92f, 3.50f, 3.60f, 4.35f, 4.83f, 5.41f, 6.00f };   // membrane modes
            default:    return { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f };
        }
    }

    struct Mode
    {
        bool on = false;
        float radius = 0, cosw = 1, sinw = 0, gain = 0;
        float y1 = 0, y2 = 0;
    };

    // A delay line one cycle long, with damping and stiffness in the loop: a plucked string, or a pipe.
    inline float waveguide (float in, const Settings& s)
    {
        const int size = (int) line.size();
        double readAt = (double) write - delaySamples;
        if (readAt < 0.0) readAt += size;
        const int i0 = (int) readAt;
        const float frac = (float) (readAt - i0);
        const float a = line[(size_t) i0], b = line[(size_t) ((i0 + 1) % size)];
        float v = a + (b - a) * frac;

        // Damping: the high end loses more on every pass, which is what makes it decay into a sine.
        damp = v + (damp - v) * dampCoef;
        v = damp;

        // Stiffness: the all-pass delays the top end a little less, so the partials spread out of tune.
        const float ap = allpassCoef * v + allpassX - allpassCoef * allpassY;
        allpassX = v;
        allpassY = ap;
        v = ap;

        // A tube reflects with its phase turned over, which leaves only the odd partials: a clarinet, not a string.
        float fed = v * loopGain;
        if (s.model == Tube) fed = -fed;

        // Struck at `position`: the input goes in at two points, which cancels the partials with a node there.
        const double tap = juce::jlimit (0.02, 0.98, (double) s.position) * delaySamples;
        double second = (double) write - tap;
        if (second < 0.0) second += size;
        line[(size_t) write] = fed + in;
        line[(size_t) ((int) second % size)] -= in * 0.5f;

        write = (write + 1) % size;

        // No DC: the loop is a comb, and a struck one can drift.
        const float out = v - dcPrev + 0.995f * dcState;
        dcPrev = v;
        dcState = out;
        // A short hit puts very little energy into the line; this brings the ringing up to where the rest
        // of the sound sits, and it is held in bounds above.
        return juce::jlimit (-8.0f, 8.0f, out * 3.0f);
    }

    // A bank of two-pole resonators: bells, plates and drum heads.
    inline float modal (float in, const Settings&)
    {
        float sum = 0;
        for (auto& m : modes)
        {
            if (! m.on) continue;
            // Direct-form resonator: y = 2 r cos(w) y1 - r^2 y2 + r sin(w) x
            const float y = 2.0f * m.radius * m.cosw * m.y1 - m.radius * m.radius * m.y2 + m.radius * m.sinw * in;
            m.y2 = m.y1;
            m.y1 = y;
            sum += y * m.gain;
        }
        return juce::jlimit (-8.0f, 8.0f, sum * 0.6f);
    }

    double sr = 44100.0, delaySamples = 200.0;
    std::vector<float> line;
    int write = 0;
    float damp = 0, dampCoef = 0.5f, allpassCoef = 0, allpassX = 0, allpassY = 0;
    float loopGain = 0.9f, seconds = 1.0f;
    float dcState = 0, dcPrev = 0;
    std::array<Mode, NumModes> modes {};
};

} // namespace ab::dsp
