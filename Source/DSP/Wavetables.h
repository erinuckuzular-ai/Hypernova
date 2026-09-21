#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <complex>
#include <functional>
#include <vector>

// Built-in wavetables. Every table is 32 single-cycle frames that morph as the POSITION knob moves.
// Frames are drawn as functions, FFT'd once, then resynthesised at several band limits (mip levels)
// so high notes don't alias.
namespace ab
{

constexpr int wtFrames = 32;
constexpr int wtBaseSize = 2048;
constexpr int wtLevels = 10;         // level l keeps harmonics 1..(512 >> l)
constexpr int wtMaxHarmonics = 512;

inline int wtLevelSize (int level)     { return juce::jmax (256, wtBaseSize >> level); }
inline int wtLevelHarmonics (int level) { return wtMaxHarmonics >> level; }

struct Wavetable
{
    juce::String name, blurb;
    std::vector<float> data;                    // every frame at every level, each with one guard sample
    std::array<size_t, wtLevels> levelOffset {}; // start of level l inside a frame block
    size_t frameStride = 0;

    const float* frame (int f, int level) const { return data.data() + (size_t) f * frameStride + levelOffset[(size_t) level]; }
};

enum TableId
{
    TAnalog, TSyncSaw, TPulse, T808, TLogDrum, TGrowl, TVowel, TDigital, TFold, THarmonic, TMetal, TScream,
    TRichSine, TGlass, TAir, // appended only: saved sessions store the table index
    NumTables
};

class WavetableBank
{
public:
    static const WavetableBank& get()
    {
        static WavetableBank bank;
        return bank;
    }

    const Wavetable& table (int i) const { return tables[(size_t) juce::jlimit (0, NumTables - 1, i)]; }

    // Builds a table from an audio file's samples: 32 single cycles taken evenly across the file, each
    // resampled to one cycle. Works for wavetable files (2048-sample cycles) and for any other audio.
    static Wavetable fromAudio (const juce::String& name, const float* samples, int numSamples, int cycleLength = 0)
    {
        Plans plans = makePlans();
        const int cycle = cycleLength > 0 ? cycleLength : juce::jmax (2, juce::jmin (wtBaseSize, numSamples / wtFrames));
        return buildFrames (name, "imported wavetable", plans, [&] (int f, float* dst)
        {
            const double start = numSamples <= cycle ? 0.0
                                                     : (double) f / (wtFrames - 1) * (double) (numSamples - cycle);
            for (int i = 0; i < wtBaseSize; ++i)
            {
                // Linear resample of one cycle up to the base size.
                const double x = start + (double) i / wtBaseSize * cycle;
                const int i0 = (int) x;
                const double fr = x - i0;
                const float a = samples[juce::jlimit (0, numSamples - 1, i0)];
                const float b = samples[juce::jlimit (0, numSamples - 1, i0 + 1)];
                dst[i] = (float) (a + (b - a) * fr);
            }
        });
    }

    static juce::StringArray names()
    {
        return { "Analog", "Sync Saw", "Pulse", "808 Body", "Log Drum", "Growl", "Vowel", "Digital", "Fold", "Harmonic", "Metal", "Scream", "Rich Sine", "Glass", "Air" };
    }

    // Smallest mip level whose harmonics all stay under the limit at this frequency.
    static int levelFor (double freq, double sampleRate)
    {
        return juce::jlimit (0, wtLevels - 1, (int) std::ceil (exactLevel (freq, sampleRate)));
    }

    // Continuous level: harmonics up to `limit` Hz. When running oversampled there's room above 20 kHz
    // (the downsampler removes it), so tables can stay full-bright right up the keyboard.
    static double exactLevel (double freq, double sampleRate)
    {
        const double limit = sampleRate > 60000.0 ? juce::jmin (0.45 * sampleRate, 36000.0) : juce::jmin (0.45 * sampleRate, 19500.0);
        const double h = limit / juce::jmax (1.0, freq);
        if (h >= wtMaxHarmonics) return 0.0;
        if (h < 1.0) return (double) (wtLevels - 1);
        return juce::jlimit (0.0, (double) (wtLevels - 1), std::log2 ((double) wtMaxHarmonics / h));
    }

private:
    using Gen = std::function<double (double t, double x)>;
    static constexpr double twoPi = juce::MathConstants<double>::twoPi;

    std::vector<Wavetable> tables;

    WavetableBank()
    {
        const auto n = names();
        auto frac = [] (double v) { return v - std::floor (v); };
        auto saw = [] (double x) { return x < 0.5 ? 2.0 * x : 2.0 * x - 2.0; }; // rising, starts at zero

        std::vector<Gen> gens (NumTables);
        std::vector<juce::String> blurbs (NumTables);

        gens[TAnalog] = [saw] (double t, double x)
        {
            const double sine = std::sin (twoPi * x);
            const double tri = x < 0.25 ? 4.0 * x : (x < 0.75 ? 2.0 - 4.0 * x : 4.0 * x - 4.0);
            const double sq = x < 0.5 ? 1.0 : -1.0;
            const double s = t * 3.0;
            if (s < 1.0) return sine + (tri - sine) * s;
            if (s < 2.0) return tri + (saw (x) - tri) * (s - 1.0);
            return saw (x) + (sq - saw (x)) * (s - 2.0);
        };
        blurbs[TAnalog] = "sine, triangle, saw, square";

        gens[TSyncSaw] = [frac] (double t, double x)
        {
            const double ratio = 1.0 + t * 7.0;
            return (2.0 * frac (x * ratio) - 1.0) * (1.0 - 0.5 * x);
        };
        blurbs[TSyncSaw] = "hard-sync sweep, the screaming lead";

        gens[TPulse] = [] (double t, double x)
        {
            const double duty = 0.5 - t * 0.46;
            return x < duty ? 1.0 : -1.0;
        };
        blurbs[TPulse] = "square thinning to a needle";

        gens[T808] = [] (double t, double x)
        {
            const double g = 1.0 + t * t * 9.0;
            const double s = std::sin (twoPi * x) + 0.35 * t * std::sin (2.0 * twoPi * x);
            return std::tanh (g * s) / std::tanh (g);
        };
        blurbs[T808] = "clean sine into fat saturated 808";

        gens[TLogDrum] = [] (double t, double x)
        {
            double v = 0.0;
            for (int h = 1; h <= 24; ++h)
            {
                const double odd = (h % 2 == 1) ? 1.0 : 0.3 * t;
                const double knock = 1.0 + 3.0 * t * std::exp (-((h - 4.0) * (h - 4.0)) / 5.0);
                v += odd * knock / std::pow ((double) h, 1.9 - 0.6 * t) * std::sin (twoPi * h * x);
            }
            return v;
        };
        blurbs[TLogDrum] = "hollow wooden knock for amapiano logs";

        gens[TGrowl] = [] (double t, double x)
        {
            const double k = t * 6.0;
            return std::sin (twoPi * x + k * std::sin (2.0 * twoPi * x) + 0.5 * k * std::sin (3.0 * twoPi * x));
        };
        blurbs[TGrowl] = "FM growl, gets angrier";

        gens[TVowel] = [] (double t, double x)
        {
            // Formants of a, e, i, o, u, measured against a 110 Hz voice.
            static const double f1[] = { 730, 530, 270, 570, 300 };
            static const double f2[] = { 1090, 1840, 2290, 840, 870 };
            const double s = t * 4.0;
            const int i = juce::jmin (3, (int) s);
            const double fr = s - i;
            const double F1 = f1[i] + (f1[i + 1] - f1[i]) * fr, F2 = f2[i] + (f2[i + 1] - f2[i]) * fr;
            double v = 0.0;
            for (int h = 1; h <= 60; ++h)
            {
                const double f = 110.0 * h;
                const double a = std::exp (-std::pow ((f - F1) / 130.0, 2.0)) + 0.7 * std::exp (-std::pow ((f - F2) / 190.0, 2.0)) + 0.08 / h;
                v += a * std::sin (twoPi * h * x);
            }
            return v;
        };
        blurbs[TVowel] = "talking a-e-i-o-u";

        gens[TDigital] = [saw] (double t, double x)
        {
            const double steps = std::round (64.0 * std::pow (3.0 / 64.0, t));
            return std::floor ((saw (x) * 0.5 + 0.5) * steps) / steps * 2.0 - 1.0;
        };
        blurbs[TDigital] = "stepped, crunchy, 8-bit";

        gens[TFold] = [] (double t, double x)
        {
            const double g = 1.0 + t * 5.0;
            return std::sin (g * std::sin (twoPi * x) * juce::MathConstants<double>::halfPi);
        };
        blurbs[TFold] = "wavefolder, buzzy and bright";

        gens[THarmonic] = [] (double t, double x)
        {
            const double h = 1.0 + t * 23.0;
            const double lo = std::floor (h), fr = h - lo;
            return std::sin (twoPi * x) + 0.7 * ((1.0 - fr) * std::sin (twoPi * lo * x) + fr * std::sin (twoPi * (lo + 1.0) * x));
        };
        blurbs[THarmonic] = "one harmonic sweeping up, organ to bell";

        gens[TMetal] = [] (double t, double x)
        {
            const double k = t * 3.5;
            return std::sin (twoPi * x + k * std::sin (7.0 * twoPi * x)) * (1.0 - 0.3 * t) + 0.3 * t * std::sin (twoPi * 5.0 * x);
        };
        blurbs[TMetal] = "clangy FM metal";

        gens[TScream] = [saw, frac] (double t, double x)
        {
            const double g = 1.0 + t * 8.0;
            const double r = 1.0 + t * 1.5;
            return juce::jlimit (-1.0, 1.0, g * saw (frac (x * r)) * (1.0 - 0.4 * x));
        };
        blurbs[TScream] = "clipped sync saw, trap lead";

        // mda DX10's carrier: a 5th-order sine approximation whose "richness" bends in odd harmonics.
        // This is the carrier of the FL Studio DX10 log drum (richness 0.151 = position 0.151).
        gens[TRichSine] = [] (double t, double x)
        {
            const double w = 0.5 - 3.0 * t * t;
            const double y = x < 0.5 ? 2.0 * x : 2.0 * x - 2.0;
            return y + y * y * y * (w * y * y - 1.0 - w);
        };
        blurbs[TRichSine] = "DX-style FM carrier, the log drum's voice";

        gens[TGlass] = [] (double t, double x)
        {
            static const int h[] = { 1, 4, 9, 13, 17, 23, 29 };
            double v = std::sin (twoPi * x);
            for (int i = 1; i < 7; ++i)
            {
                const double centre = 1.0 + t * 6.0;
                const double a = std::exp (-std::pow ((i - centre) / 1.6, 2.0)) * 0.8;
                v += a * std::sin (twoPi * h[i] * x + i * 0.7);
            }
            return v;
        };
        blurbs[TGlass] = "sparse glassy partials, crystal pads";

        gens[TAir] = [] (double t, double x)
        {
            static const auto table = []
            {
                std::array<std::pair<double, double>, 96> hp {};
                juce::Random r (1984);
                for (auto& e : hp) e = { 0.5 + 0.5 * r.nextDouble(), r.nextDouble() * twoPi };
                return hp;
            }();
            double v = 0.0;
            for (int h = 1; h <= 96; ++h)
            {
                const double tilt = std::exp (-(double) h / (3.0 + 70.0 * t * t));
                v += tilt * table[(size_t) h - 1].first / std::sqrt ((double) h) * std::sin (twoPi * h * x + table[(size_t) h - 1].second);
            }
            return v;
        };
        blurbs[TAir] = "dense breathy partials, dark to shimmering";

        // One FFT plan (and its scale) per mip level, shared by every frame of every table: making plans per
        // frame was most of the plug-in's open time, which matters on older Intel Macs.
        Plans plans = makePlans();
        for (int i = 0; i < NumTables; ++i)
            tables.push_back (build (n[i], blurbs[(size_t) i], gens[(size_t) i], plans));
    }

    struct Plans
    {
        juce::dsp::FFT fwd { 11 }; // 2048
        std::vector<std::unique_ptr<juce::dsp::FFT>> inv;
        std::vector<float> scale;
    };

    static Plans makePlans()
    {
        Plans plans;
        for (int l = 0; l < wtLevels; ++l)
        {
            const int size = wtLevelSize (l);
            plans.inv.push_back (std::make_unique<juce::dsp::FFT> ((int) std::round (std::log2 ((double) size))));
            plans.scale.push_back (inverseScale (*plans.inv.back(), size));
        }
        return plans;
    }


    static Wavetable build (const juce::String& name, const juce::String& blurb, const Gen& gen, Plans& plans)
    {
        return buildFrames (name, blurb, plans, [&gen] (int f, float* dst)
        {
            const double t = (double) f / (wtFrames - 1);
            for (int i = 0; i < wtBaseSize; ++i) dst[i] = (float) gen (t, (double) i / wtBaseSize);
        });
    }

    // Shared table construction: `fill(frame, dst)` writes one single cycle of wtBaseSize samples.
    template <typename FillFrame>
    static Wavetable buildFrames (const juce::String& name, const juce::String& blurb, Plans& plans, FillFrame&& fill)
    {
        Wavetable wt;
        wt.name = name;
        wt.blurb = blurb;
        size_t offset = 0;
        for (int l = 0; l < wtLevels; ++l)
        {
            wt.levelOffset[(size_t) l] = offset;
            offset += (size_t) wtLevelSize (l) + 1;
        }
        wt.frameStride = offset;
        wt.data.assign (wt.frameStride * wtFrames, 0.0f);

        using C = std::complex<float>;
        std::vector<C> in ((size_t) wtBaseSize), spec ((size_t) wtBaseSize), s ((size_t) wtBaseSize), out ((size_t) wtBaseSize);
        std::vector<C> harm ((size_t) wtMaxHarmonics + 1);

        std::vector<float> frame ((size_t) wtBaseSize);
        for (int f = 0; f < wtFrames; ++f)
        {
            fill (f, frame.data());
            for (int i = 0; i < wtBaseSize; ++i)
                in[(size_t) i] = C (frame[(size_t) i], 0.0f);
            plans.fwd.perform (in.data(), spec.data(), false);

            // Harmonic k as complex amplitude: x(n) = Re(sum c_k e^{i 2pi k n / N}) with c_k = 2 X_k / N.
            for (int k = 1; k <= wtMaxHarmonics; ++k)
                harm[(size_t) k] = spec[(size_t) k] * (2.0f / (float) wtBaseSize);

            float gain = 1.0f;
            for (int l = 0; l < wtLevels; ++l)
            {
                const int size = wtLevelSize (l), maxH = wtLevelHarmonics (l);
                float* dst = wt.data.data() + (size_t) f * wt.frameStride + wt.levelOffset[(size_t) l];
                // Direct resynthesis via inverse FFT at this level's size.
                std::fill (s.begin(), s.begin() + size, C());
                for (int k = 1; k <= maxH && k < size / 2; ++k)
                    s[(size_t) k] = harm[(size_t) k];
                plans.inv[(size_t) l]->perform (s.data(), out.data(), true);
                // JUCE's inverse may or may not scale by 1/N; the scale was recovered from a known unit harmonic.
                const float scale = plans.scale[(size_t) l];
                for (int i = 0; i < size; ++i)
                    dst[i] = out[(size_t) i].real() * scale;

                if (l == 0)
                {
                    float peak = 0.0f;
                    for (int i = 0; i < size; ++i)
                        peak = juce::jmax (peak, std::abs (dst[i]));
                    gain = peak > 1.0e-6f ? 1.0f / peak : 1.0f;
                }
                for (int i = 0; i < size; ++i)
                    dst[i] *= gain;
                dst[size] = dst[0];
            }
        }
        return wt;
    }

    // Multiplier that turns inv.perform(harmonic c at bin 1) into Re(c e^{...}) with unit amplitude.
    static float inverseScale (juce::dsp::FFT& inv, int size)
    {
        using C = std::complex<float>;
        std::vector<C> s ((size_t) size), out ((size_t) size);
        s[1] = C (1.0f, 0.0f);
        inv.perform (s.data(), out.data(), true);
        return out[0].real() != 0.0f ? 1.0f / out[0].real() : 1.0f;
    }
};

} // namespace ab
